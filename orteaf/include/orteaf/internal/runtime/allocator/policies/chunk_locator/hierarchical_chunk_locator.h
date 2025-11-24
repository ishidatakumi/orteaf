#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <new>
#include <stdexcept>
#include <vector>

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/backend/backend_traits.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/base/strong_id.h"
#include "orteaf/internal/runtime/allocator/memory_block.h"
#include "orteaf/internal/diagnostics/log/log.h"
#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::runtime::allocator::policies {

// 階層的にサイズクラスを並べたチャンクロケータ。
// levels に指定されたチャンクサイズ（大きい順）ごとにフリーリストを持ち、
// 最小で足りる層から割り当てを行う。親子分割の完全実装はこれからだが、
// まずは複数サイズクラスを扱える骨格として提供する。
template <class Resource, ::orteaf::internal::backend::Backend B>
class HierarchicalChunkLocator {
public:
    using BufferId = ::orteaf::internal::base::BufferId;
    using BufferView = typename ::orteaf::internal::backend::BackendTraits<B>::BufferView;
    using MemoryBlock = ::orteaf::internal::runtime::allocator::MemoryBlock<B>;
    using Device = typename ::orteaf::internal::backend::BackendTraits<B>::Device;
    using Context = typename ::orteaf::internal::backend::BackendTraits<B>::Context;
    using Stream = typename ::orteaf::internal::backend::BackendTraits<B>::Stream;
    struct Config {
        Device device{};
        Context context{};
        Stream stream{};
        // 大きい順で渡すことを想定する。例: {1_MB, 256_KB, 64_KB}
        std::vector<std::size_t> levels;
    };

    void initialize(const Config& cfg, Resource* resource) {
        cfg_ = cfg;
        resource_ = resource;
        // resource は外部所有。nullptr は受け付けない。
        if (resource_ == nullptr) {
            using namespace orteaf::internal::diagnostics::error;
            throwError(OrteafErrc::InvalidArgument, "HierarchicalChunkLocator requires non-null Resource*");
        }
        layers_.clear();
        layers_.reserve(cfg.levels.size());
        for (auto sz : cfg.levels) {
            layers_.push_back(Layer{sz});
        }
    }

    // 最小で足りるサイズクラスから MemoryBlock を返す。
    MemoryBlock allocate(std::size_t size, std::size_t alignment = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto target_layer = pickLayer(size);
        if (target_layer == kInvalidLayer || resource_ == nullptr) {
            using namespace orteaf::internal::diagnostics::error;
            throwError(OrteafErrc::OutOfMemory, "No suitable layer or resource is null");
        }

        ensureFreeSlot(target_layer, alignment);

        auto& L = layers_[target_layer];
        const auto slot = popFree(L.free_list);
        Slot& s = L.slots[slot];
        s.state = State::InUse;
        if (!s.mapped) {
            s.view = resource_->map(s.view, cfg_.device, cfg_.context, cfg_.stream);
            s.mapped = true;
        }
        return MemoryBlock{encode(target_layer, static_cast<uint32_t>(slot)), s.view};
    }

    void deallocate(BufferId id, std::size_t size = 0, std::size_t alignment = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto [layer, slot] = decode(id);
        if (layer >= layers_.size()) return;
        auto& L = layers_[layer];
        if (slot >= L.slots.size()) return;

        Slot& s = L.slots[slot];
        if (s.state != State::InUse) return;

        // map/unmap を分離。CPU では unmap が deallocate を兼ねる。
        resource_->unmap(s.view, size ? size : L.chunk_size, alignment, cfg_.device, cfg_.context, cfg_.stream);
        s.mapped = false;
        s.state = State::Free;
        L.free_list.pushBack(slot);

        // 親があれば、親配下の子がすべて Free ならマージする
        if (s.parent_slot != kNoParent && layer > 0) {
            tryMergeParent(layer, layer - 1, s.parent_slot);
        }
    }

    std::size_t findChunkSize(BufferId id) const {
        auto [layer, slot] = decode(id);
        if (layer >= layers_.size()) return 0;
        const auto& L = layers_[layer];
        if (slot >= L.slots.size()) return 0;
        return L.chunk_size;
    }

private:
    enum class State : uint8_t { Free, InUse, Split };
    static constexpr uint32_t kNoParent = UINT32_MAX;
    static constexpr uint32_t kNoChild = UINT32_MAX;
    static constexpr uint32_t kInvalidLayer = UINT32_MAX;
    static constexpr BufferId::underlying_type kLargeMask = BufferId::underlying_type{1u} << 31;
    static constexpr uint32_t kLayerBits = 8;          // layer を 8bit まで許容
    static constexpr uint32_t kSlotBits = 31 - kLayerBits;  // BufferId の下位にスロット
    static constexpr uint32_t kSlotMask = (uint32_t{1u} << kSlotBits) - 1u;

    struct Slot {
        BufferView view{};
        State state{State::Free};
        bool mapped{false};
        uint32_t parent_slot{kNoParent};
        uint32_t child_layer{kNoChild};
        uint32_t child_begin{0};
#if ORTEAF_CORE_DEBUG_ENABLED
        uint32_t child_count{0};
#endif
    };

    // span_free メタ情報: デバッグ時は count も保持、リリース時は begin のみ。
#if ORTEAF_CORE_DEBUG_ENABLED
    using SpanFreeEntry = std::pair<uint32_t, uint32_t>;  // {begin, count}
#else
    using SpanFreeEntry = uint32_t;  // begin のみ
#endif

    struct Layer {
        explicit Layer(std::size_t sz) : chunk_size(sz) {}
        std::size_t chunk_size{0};
        base::HeapVector<Slot> slots;
        base::HeapVector<uint32_t> free_list;
        base::HeapVector<SpanFreeEntry> span_free;
    };

    uint32_t pickLayer(std::size_t req) const {
        uint32_t best = kInvalidLayer;
        for (uint32_t i = 0; i < layers_.size(); ++i) {
            if (req <= layers_[i].chunk_size) {
                best = i;  // levels は大きい順なので最初に当たった層が最小で足りる
                break;
            }
        }
        return best;
    }

    static uint32_t popFree(base::HeapVector<uint32_t>& free_list) {
        const std::size_t idx = free_list.size() - 1;
        const uint32_t value = free_list[idx];
        free_list.resize(idx);
        return value;
    }

    // ルート層の VA を連続で拡張する。子層は必ず親の split で生成する。
    void addRegion(std::size_t bytes) {
        if (layers_.empty()) {
            using namespace orteaf::internal::diagnostics::error;
            throwError(OrteafErrc::InvalidState, "No layers configured");
        }
        auto& root = layers_[0];
        const std::size_t chunk_size = root.chunk_size;
        std::size_t remaining = bytes == 0 ? chunk_size : bytes;

        BufferView base = resource_->reserve(remaining, /*alignment=*/0, cfg_.device, cfg_.stream);
        std::size_t offset = 0;
        while (remaining > 0) {
            const std::size_t step = remaining >= chunk_size ? chunk_size : remaining;
            const std::size_t slot = root.slots.size();
            BufferView view{reinterpret_cast<char*>(base.data()) + offset, base.offset() + offset, step};
            root.slots.emplaceBack(Slot{view, State::Free, false, kNoParent, kNoChild, 0, 0});
            root.free_list.pushBack(static_cast<uint32_t>(slot));
            offset += step;
            remaining -= step;
        }
    }

    // target_layer に free がなければ、上位を探して段階的に split する。
    void ensureFreeSlot(uint32_t target_layer, std::size_t alignment) {
        if (target_layer >= layers_.size()) {
            using namespace orteaf::internal::diagnostics::error;
            throwError(OrteafErrc::OutOfRange, "Layer index out of range");
        }
        if (!layers_[target_layer].free_list.empty()) return;

        // 上位層を遡って空きを探す
        int parent = static_cast<int>(target_layer) - 1;
        for (; parent >= 0; --parent) {
            if (!layers_[parent].free_list.empty()) break;
        }

        // ルートにも無ければ新規リージョンを追加
        if (parent < 0) {
            addRegion(layers_[0].chunk_size);
            parent = 0;
        }

        // 見つかった親から target_layer まで段階的に split
        for (uint32_t layer = static_cast<uint32_t>(parent); layer < target_layer; ++layer) {
            const uint32_t child = layer + 1;
            if (!layers_[child].free_list.empty()) continue;
            if (layers_[layer].free_list.empty()) {
            using namespace orteaf::internal::diagnostics::error;
                throwError(OrteafErrc::OutOfMemory, "Failed to refill parent layer");
            }
            splitOne(layer, child);
        }

        if (layers_[target_layer].free_list.empty()) {
            using namespace orteaf::internal::diagnostics::error;
            throwError(OrteafErrc::OutOfMemory, "Failed to ensure free slot");
        }
    }

    // 親slotを取り出し、child_sizeのスロットを (parent_size / child_size) 個生成して子層へ。
    // levels[i] % levels[i+1] == 0 を前提。
    void splitOne(uint32_t parent_layer, uint32_t child_layer) {
        auto& P = layers_[parent_layer];
        auto& C = layers_[child_layer];
        if (P.free_list.empty()) return;

        const uint32_t parent_slot = popFree(P.free_list);
        Slot& ps = P.slots[parent_slot];
        ps.state = State::Split;

        const std::size_t parent_size = P.chunk_size;
        const std::size_t child_size = C.chunk_size;
        if (child_size == 0 || parent_size % child_size != 0) {
            using namespace orteaf::internal::diagnostics::error;
            throwError(OrteafErrc::InvalidParameter, "Non-divisible layer sizes");
        }
        const std::size_t count = parent_size / child_size;
#if ORTEAF_CORE_DEBUG_ENABLED
        // 連続分割前提を守れているか追加検証
        if (count == 0) {
            using namespace orteaf::internal::diagnostics::error;
            throwError(OrteafErrc::InvalidParameter, "Split count is zero");
        }
#endif

        // 連続ブロックの再利用を優先
        std::size_t base_slot = C.slots.size();
        bool reused = false;
#if ORTEAF_CORE_DEBUG_ENABLED
        for (std::size_t i = 0; i < C.span_free.size(); ++i) {
            auto [beg, cnt] = C.span_free[i];
            if (cnt == count) {
                base_slot = beg;
                C.span_free[i] = C.span_free.back();
                C.span_free.resize(C.span_free.size() - 1);
                reused = true;
                break;
            }
        }
#else
        if (!C.span_free.empty()) {
            base_slot = C.span_free.back();
            C.span_free.resize(C.span_free.size() - 1);
            reused = true;
        }
#endif
        if (!reused) {
            const std::size_t old = C.slots.size();
            C.slots.resize(old + count);
            base_slot = old;
        }
        for (std::size_t i = 0; i < count; ++i) {
            const std::size_t off = i * child_size;
            BufferView v{reinterpret_cast<char*>(ps.view.data()) + off,
                         ps.view.offset() + off,
                         child_size};
            Slot& cs = C.slots[base_slot + i];
            cs.view = v;
            cs.state = State::Free;
            cs.mapped = false;
            cs.parent_slot = parent_slot;
            cs.child_layer = kNoChild;
            cs.child_begin = 0;
            cs.child_count = 0;
            C.free_list.pushBack(static_cast<uint32_t>(base_slot + i));
        }

        ps.child_layer = child_layer;
        ps.child_begin = static_cast<uint32_t>(base_slot);
#if ORTEAF_CORE_DEBUG_ENABLED
        ps.child_count = static_cast<uint32_t>(count);
#endif
    }

    // 子層で全Freeなら親をFreeに戻し、子スロット範囲を span_free に返す
    void tryMergeParent(uint32_t child_layer, uint32_t parent_layer, uint32_t parent_slot) {
        auto& P = layers_[parent_layer];
        if (parent_slot >= P.slots.size()) return;
        Slot& ps = P.slots[parent_slot];
        if (ps.state != State::Split || ps.child_layer != child_layer) return;

        const uint32_t begin = ps.child_begin;
#if ORTEAF_CORE_DEBUG_ENABLED
        const uint32_t count = ps.child_count;
        // 整合性チェック: デバッグ時は保持している child_count とサイズ比を比較
        const std::size_t expected = layers_[parent_layer].chunk_size / layers_[child_layer].chunk_size;
        if (count != expected) {
            using namespace orteaf::internal::diagnostics::error;
            throwError(OrteafErrc::InvalidState, "Child count mismatch on merge");
        }
#else
        // リリース時はサイズ比から算出
        const uint32_t count = static_cast<uint32_t>(layers_[parent_layer].chunk_size / layers_[child_layer].chunk_size);
#endif
        auto& C = layers_[child_layer];
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t idx = begin + i;
            if (idx >= C.slots.size()) return;
            if (C.slots[idx].state != State::Free) return;
        }

        // 子範囲を再利用可能な span として戻す
#if ORTEAF_CORE_DEBUG_ENABLED
        C.span_free.pushBack({begin, count});
#else
        C.span_free.pushBack(begin);
#endif

        // 親を Free に戻す
        ps.state = State::Free;
        ps.child_layer = kNoChild;
        ps.child_begin = 0;
#if ORTEAF_CORE_DEBUG_ENABLED
        ps.child_count = 0;
#endif
        P.free_list.pushBack(parent_slot);
    }

    BufferId encode(uint32_t layer, uint32_t slot) const {
        const auto layer_part = (layer & ((uint32_t{1u} << kLayerBits) - 1u)) << kSlotBits;
        const auto slot_part = slot & kSlotMask;
        return BufferId{static_cast<BufferId::underlying_type>(layer_part | slot_part)};
    }

    std::pair<uint32_t, uint32_t> decode(BufferId id) const {
        const auto raw_full = static_cast<BufferId::underlying_type>(id);
#if ORTEAF_CORE_DEBUG_ENABLED
        if ((raw_full & kLargeMask) != 0) {
            using namespace orteaf::internal::diagnostics::error;
            // LargeAlloc 用の ID が紛れ込んでいる場合はデバッグで検出
            throwError(OrteafErrc::InvalidParameter, "LargeAlloc BufferId passed to chunk locator");
        }
#endif
        const auto raw = static_cast<uint32_t>(raw_full);
        const uint32_t slot = raw & kSlotMask;
        const uint32_t layer = (raw >> kSlotBits) & ((uint32_t{1u} << kLayerBits) - 1u);
        return {layer, slot};
    }

private:
    Config cfg_{};
    Resource* resource_{nullptr};  // 非所有
    std::vector<Layer> layers_{};
    mutable std::mutex mutex_{};
};

}  // namespace orteaf::internal::runtime::allocator::policies

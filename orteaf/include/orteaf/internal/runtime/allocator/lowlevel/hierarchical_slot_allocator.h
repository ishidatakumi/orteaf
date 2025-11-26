#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/backend/backend_traits.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/diagnostics/error/error_macros.h"

namespace orteaf::internal::runtime::allocator::policies {

/**
 * @brief 階層的スロットアロケータ。
 *
 * levelsに指定されたスロットサイズ（大きい順）ごとにフリーリストを持ち、
 * 最小で足りる層から割り当てを行う。大きい層のスロットを分割して
 * 小さい層のスロットを作る親子関係を持つ。
 */
template <class HeapOps, ::orteaf::internal::backend::Backend B>
class HierarchicalSlotAllocator {
public:
    using BufferView = typename ::orteaf::internal::backend::BackendTraits<B>::BufferView;
    using HeapRegion = typename ::orteaf::internal::backend::BackendTraits<B>::HeapRegion;

    enum class State : uint8_t { Free, InUse, Split };

    struct Config {
        std::vector<std::size_t> levels;  // 大きい順: {1MB, 256KB, 64KB}
        std::size_t initial_bytes{0};
        std::size_t region_multiplier{1};
    };

    void initialize(const Config& config, HeapOps* heap_ops) {
        config_ = config;
        heap_ops_ = heap_ops;

        ORTEAF_THROW_IF_NULL(heap_ops_, "HierarchicalSlotAllocator requires non-null HeapOps*");

        layers_.clear();
        for (auto size : config.levels) {
            layers_.emplace_back(size);
        }

        std::size_t initial = config.initial_bytes;
        if (initial == 0 && !config.levels.empty()) {
            initial = config.levels[0];
        }
        if (initial > 0) {
            addRegion(initial);
        }
    }

    BufferView allocate(std::size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);

        uint32_t target = pickLayer(config_.levels, size);
        ORTEAF_THROW_IF(target == kInvalidLayer, OutOfMemory, "No suitable layer");

        ensureSlotAvailable(target);
        uint32_t slot_idx = acquireSlot(target);
        return mapSlot(target, slot_idx);
    }

    void deallocate(BufferView view) {
        if (!view) return;
        std::lock_guard<std::mutex> lock(mutex_);

        auto [layer_idx, slot_idx] = findSlot(view);
        if (layer_idx == kInvalidLayer) return;

        Slot& slot = layers_[layer_idx].slots[slot_idx];
        if (slot.state != State::InUse) return;

        unmapSlot(layer_idx, slot_idx);
        releaseSlot(layer_idx, slot_idx);
        tryMergeUpward(layer_idx, slot_idx);
    }

private:
    static constexpr uint32_t kNoParent = UINT32_MAX;
    static constexpr uint32_t kInvalidLayer = UINT32_MAX;

    struct Slot {
        HeapRegion region{};
        State state{State::Free};
        uint32_t parent_slot{kNoParent};
        uint32_t child_begin{0};
    };

    struct Layer {
        explicit Layer(std::size_t size) : slot_size(size) {}
        std::size_t slot_size{0};
        base::HeapVector<Slot> slots;
        base::HeapVector<uint32_t> free_list;
    };

    // ========================================================================
    // Layer 1: Pure functions
    // ========================================================================

    static uint32_t pickLayer(const std::vector<std::size_t>& levels, std::size_t size) noexcept {
        uint32_t best = kInvalidLayer;
        for (uint32_t i = 0; i < levels.size(); ++i) {
            if (size <= levels[i]) {
                best = i;
            } else {
                break;
            }
        }
        return best;
    }

    static bool hasFreeSlot(const Layer& layer) noexcept {
        return !layer.free_list.empty();
    }

    static uint32_t popFreeSlot(Layer& layer) {
        const auto last = layer.free_list.size() - 1;
        const auto value = layer.free_list[last];
        layer.free_list.resize(last);
        return value;
    }

    static void markSlotInUse(Slot& slot) noexcept {
        slot.state = State::InUse;
    }

    static void markSlotFree(Slot& slot) noexcept {
        slot.state = State::Free;
    }

    static void markSlotSplit(Slot& slot, uint32_t child_begin) noexcept {
        slot.state = State::Split;
        slot.child_begin = child_begin;
    }

    static bool allSiblingsFree(const Layer& child_layer, uint32_t begin, std::size_t count) noexcept {
        for (std::size_t i = 0; i < count; ++i) {
            if (child_layer.slots[begin + i].state != State::Free) {
                return false;
            }
        }
        return true;
    }

    static void splitSlot(Layer& parent, Layer& child, uint32_t parent_slot_idx) {
        Slot& parent_slot = parent.slots[parent_slot_idx];

        const std::size_t count = parent.slot_size / child.slot_size;
        const auto child_begin = static_cast<uint32_t>(child.slots.size());

        for (std::size_t i = 0; i < count; ++i) {
            Slot new_slot{};
            new_slot.region = HeapRegion{
                static_cast<void*>(static_cast<char*>(parent_slot.region.data()) + i * child.slot_size),
                child.slot_size
            };
            new_slot.state = State::Free;
            new_slot.parent_slot = parent_slot_idx;

            child.slots.emplaceBack(new_slot);
            child.free_list.pushBack(static_cast<uint32_t>(child_begin + i));
        }

        markSlotSplit(parent_slot, child_begin);
    }

    // ========================================================================
    // Layer 2: Meaningful operations
    // ========================================================================

    void ensureSlotAvailable(uint32_t target_layer) {
        if (hasFreeSlot(layers_[target_layer])) {
            return;
        }

        int parent_layer = findParentWithFreeSlot(target_layer);

        if (parent_layer < 0) {
            const std::size_t multiplier = (config_.region_multiplier == 0) ? 1 : config_.region_multiplier;
            addRegion(layers_[0].slot_size * multiplier);
            parent_layer = 0;
        }

        for (int i = parent_layer; i < static_cast<int>(target_layer); ++i) {
            uint32_t slot_idx = popFreeSlot(layers_[i]);
            splitSlot(layers_[i], layers_[i + 1], slot_idx);
        }
    }

    uint32_t acquireSlot(uint32_t layer_index) {
        Layer& layer = layers_[layer_index];
        uint32_t slot_idx = popFreeSlot(layer);
        markSlotInUse(layer.slots[slot_idx]);
        return slot_idx;
    }

    BufferView mapSlot(uint32_t layer_index, uint32_t slot_index) {
        Slot& slot = layers_[layer_index].slots[slot_index];
        return heap_ops_->map(slot.region);
    }

    void unmapSlot(uint32_t layer_index, uint32_t slot_index) {
        Layer& layer = layers_[layer_index];
        Slot& slot = layer.slots[slot_index];
        heap_ops_->unmap(slot.region, layer.slot_size);
    }

    void releaseSlot(uint32_t layer_index, uint32_t slot_index) {
        Layer& layer = layers_[layer_index];
        markSlotFree(layer.slots[slot_index]);
        layer.free_list.pushBack(slot_index);
    }

    std::pair<uint32_t, uint32_t> findSlot(BufferView view) const {
        for (uint32_t layer_idx = 0; layer_idx < layers_.size(); ++layer_idx) {
            const auto& layer = layers_[layer_idx];
            for (uint32_t slot_idx = 0; slot_idx < layer.slots.size(); ++slot_idx) {
                const Slot& slot = layer.slots[slot_idx];
                if (slot.state == State::InUse && slot.region.data() == view.data()) {
                    return {layer_idx, slot_idx};
                }
            }
        }
        return {kInvalidLayer, 0};
    }

    void tryMergeUpward(uint32_t layer_idx, uint32_t slot_idx) {
        Slot& slot = layers_[layer_idx].slots[slot_idx];
        if (slot.parent_slot == kNoParent || layer_idx == 0) return;

        uint32_t parent_layer_idx = layer_idx - 1;
        uint32_t parent_slot_idx = slot.parent_slot;

        Layer& parent_layer = layers_[parent_layer_idx];
        Layer& child_layer = layers_[layer_idx];
        Slot& parent = parent_layer.slots[parent_slot_idx];

        if (parent.state != State::Split) return;

        const std::size_t count = parent_layer.slot_size / child_layer.slot_size;
        if (!allSiblingsFree(child_layer, parent.child_begin, count)) return;

        // 子をfree_listから除去
        base::HeapVector<uint32_t> new_free_list;
        for (std::size_t i = 0; i < child_layer.free_list.size(); ++i) {
            const auto idx = child_layer.free_list[i];
            if (idx < parent.child_begin || idx >= parent.child_begin + count) {
                new_free_list.pushBack(idx);
            }
        }
        child_layer.free_list = std::move(new_free_list);

        // 親をFreeに戻す
        markSlotFree(parent);
        parent.child_begin = 0;
        parent_layer.free_list.pushBack(parent_slot_idx);

        // 再帰的に上へ
        tryMergeUpward(parent_layer_idx, parent_slot_idx);
    }

    int findParentWithFreeSlot(uint32_t target_layer) const noexcept {
        for (int i = static_cast<int>(target_layer) - 1; i >= 0; --i) {
            if (hasFreeSlot(layers_[i])) {
                return i;
            }
        }
        return -1;
    }

    // ========================================================================
    // Helper functions
    // ========================================================================

    void addRegion(std::size_t bytes) {
        ORTEAF_THROW_IF(layers_.empty(), InvalidState, "No layers configured");

        auto& root = layers_[0];
        std::size_t remaining = (bytes == 0) ? root.slot_size : bytes;

        HeapRegion base_region = heap_ops_->reserve(remaining);
        std::size_t offset = 0;

        while (remaining > 0) {
            const std::size_t step = (remaining >= root.slot_size) ? root.slot_size : remaining;
            const auto slot_index = static_cast<uint32_t>(root.slots.size());

            Slot slot{};
            slot.region = HeapRegion{
                static_cast<void*>(static_cast<char*>(base_region.data()) + offset),
                step
            };
            slot.state = State::Free;

            root.slots.emplaceBack(slot);
            root.free_list.pushBack(slot_index);

            offset += step;
            remaining -= step;
        }
    }

    // ========================================================================
    // Member variables
    // ========================================================================
    Config config_{};
    HeapOps* heap_ops_{nullptr};
    std::vector<Layer> layers_;
    mutable std::mutex mutex_;
};

}  // namespace orteaf::internal::runtime::allocator::policies

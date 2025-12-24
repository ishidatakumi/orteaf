#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/base/math_utils.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/diagnostics/error/error_macros.h"
#include "orteaf/internal/diagnostics/log/log.h"
#include "orteaf/internal/execution/base/backend_traits.h"

namespace orteaf::internal::execution::allocator::policies {

// ============================================================================
// SpanFreeEntry: デバッグ時は {begin, count}、リリース時は begin のみ
// ============================================================================
namespace detail {

template <bool DebugEnabled> struct SpanFreeEntryTraits;

template <> struct SpanFreeEntryTraits<false> {
  using Type = uint32_t;

  static constexpr uint32_t begin(Type entry) noexcept { return entry; }

  static constexpr uint32_t count(Type /*entry*/, std::size_t parent_size,
                                  std::size_t child_size) noexcept {
    return static_cast<uint32_t>(parent_size / child_size);
  }

  static constexpr Type make(uint32_t b, uint32_t /*c*/) noexcept { return b; }

  static constexpr bool matchesCount(Type /*entry*/,
                                     uint32_t /*expected*/) noexcept {
    // リリース時は count を保持しないため、常に true（サイズ比から算出）
    return true;
  }
};

template <> struct SpanFreeEntryTraits<true> {
  using Type = std::pair<uint32_t, uint32_t>;

  static constexpr uint32_t begin(Type entry) noexcept { return entry.first; }

  static constexpr uint32_t count(Type entry, std::size_t /*parent_size*/,
                                  std::size_t /*child_size*/) noexcept {
    return entry.second;
  }

  static constexpr Type make(uint32_t b, uint32_t c) noexcept { return {b, c}; }

  static constexpr bool matchesCount(Type entry, uint32_t expected) noexcept {
    return entry.second == expected;
  }
};

// ============================================================================
// SlotBase: デバッグ時のみ child_count を保持
// ============================================================================
template <bool DebugEnabled, typename Region, typename BufferView>
struct SlotBase {
  Region region{};
  BufferView mapped{};
  uint8_t state{0}; // State enum value
  bool mapped_flag{false};
  uint32_t parent_slot{UINT32_MAX};
  uint32_t child_layer{UINT32_MAX};
  uint32_t child_begin{0};
};

template <typename Region, typename BufferView>
struct SlotBase<true, Region, BufferView> {
  Region region{};
  BufferView mapped{};
  uint8_t state{0};
  bool mapped_flag{false};
  uint32_t parent_slot{UINT32_MAX};
  uint32_t child_layer{UINT32_MAX};
  uint32_t child_begin{0};
  uint32_t child_count{0}; // デバッグ時のみ
};

// child_count のアクセサ（リリース時はサイズ比から算出）
template <bool DebugEnabled> struct SlotChildCountAccessor {
  template <typename Slot>
  static void set(Slot & /*slot*/, uint32_t /*count*/) noexcept {}

  template <typename Slot>
  static uint32_t get(const Slot & /*slot*/, std::size_t parent_size,
                      std::size_t child_size) noexcept {
    return static_cast<uint32_t>(parent_size / child_size);
  }
};

template <> struct SlotChildCountAccessor<true> {
  template <typename Slot>
  static void set(Slot &slot, uint32_t count) noexcept {
    slot.child_count = count;
  }

  template <typename Slot>
  static uint32_t get(const Slot &slot, std::size_t /*parent_size*/,
                      std::size_t /*child_size*/) noexcept {
    return slot.child_count;
  }
};

} // namespace detail

// ============================================================================
// HierarchicalSlotAllocator
// ============================================================================

/**
 * @brief 階層的にサイズクラスを並べたチャンクロケータ。
 *
 * levels に指定されたチャンクサイズ（大きい順）ごとにフリーリストを持ち、
 * 最小で足りる層から割り当てを行う。親子分割の完全実装はこれからだが、
 * まずは複数サイズクラスを扱える骨格として提供する。
 */
template <class HeapOps, ::orteaf::internal::backend::Backend B>
class HierarchicalSlotAllocator {
public:
  // ========================================================================
  // Type aliases
  // ========================================================================
  using BufferView =
      typename ::orteaf::internal::execution::base::BackendTraits<B>::BufferView;
  using HeapRegion =
      typename ::orteaf::internal::execution::base::BackendTraits<B>::HeapRegion;

  enum class State : uint8_t { Free, InUse, Split };

  /**
   * @brief HierarchicalSlotAllocator 固有の設定。
   */
  struct Config {
    /// 大きい順で渡すことを想定する。例: {1_MB, 256_KB, 64_KB}
    std::vector<std::size_t> levels;
    /// ルート初期確保サイズ（0なら chunk_size 1個分）
    std::size_t initial_bytes{0};
    /// 追加確保時の倍率（chunk_size * region_multiplier を一度に確保）
    std::size_t region_multiplier{1};
    /// levels の閾値（2の冪乗）。threshold 以下は 2
    /// の冪乗で刻む運用を前提とする。
    std::size_t threshold{};
  };

  // ========================================================================
  // Public API
  // ========================================================================

  void initialize(const Config &config, HeapOps *heap_ops) {
    config_ = config;
    heap_ops_ = heap_ops;

    ORTEAF_THROW_IF_NULL(
        heap_ops_, "HierarchicalSlotAllocator requires non-null HeapOps*");
    validateLevels(config.levels, config.threshold);

    layers_.clear();
    layers_.reserve(config.levels.size());
    for (auto chunk_size : config.levels) {
      layers_.push_back(Layer{chunk_size});
    }

    const std::size_t initial = computeInitialBytes(config);
    if (initial > 0) {
      addRegion(initial);
    }
  }

  /**
   * @brief 指定サイズに対応するスロットを割り当て、BufferViewを返す。
   * @param size 要求サイズ
   * @return 割り当てられたBufferView
   */
  BufferView allocate(std::size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto target_layer = pickLayer(size);
    ORTEAF_THROW_IF(target_layer == kInvalidLayer || heap_ops_ == nullptr,
                    OutOfMemory, "No suitable layer or heap_ops is null");

    ensureFreeSlot(target_layer);

    auto &layer = layers_[target_layer];
    const auto slot_index = popFree(layer.free_list);
    Slot &slot = layer.slots[slot_index];

    slot.state = static_cast<uint8_t>(State::InUse);

    if (!slot.mapped_flag) {
      slot.mapped = heap_ops_->map(slot.region);
      slot.mapped_flag = true;
    }

    return slot.mapped;
  }

  /**
   * @brief BufferViewに対応するスロットを解放し、物理メモリをunmapする。
   * @param view 解放するBufferView
   */
  void deallocate(BufferView view) {
    if (!view)
      return;
    std::lock_guard<std::mutex> lock(mutex_);

    auto [layer_index, slot_index] = findSlotByAddress(view.data());
    if (layer_index == kInvalidLayer)
      return;

    auto &layer = layers_[layer_index];
    if (slot_index >= layer.slots.size())
      return;

    Slot &slot = layer.slots[slot_index];
    if (getState(slot) != State::InUse)
      return;

    heap_ops_->unmap(slot.mapped, layer.chunk_size);
    resetSlot(slot);
    layer.free_list.pushBack(slot_index);

    // 親があれば、親配下の子がすべて Free ならマージする
    if (slot.parent_slot != kNoParent && layer_index > 0) {
      tryMergeParent(layer_index, layer_index - 1, slot.parent_slot);
    }
  }

  // ========================================================================
  // Debug support
  // ========================================================================
#if ORTEAF_CORE_DEBUG_ENABLED
  using DebugSpanFreeEntry = std::pair<uint32_t, uint32_t>;

  struct SlotSnapshot {
    State state{};
    bool mapped{};
    uint32_t parent_slot{};
    uint32_t child_layer{};
    uint32_t child_begin{};
    uint32_t child_count{};
  };

  struct LayerSnapshot {
    std::size_t chunk_size{};
    std::vector<SlotSnapshot> slots{};
    std::vector<uint32_t> free_list{};
    std::vector<DebugSpanFreeEntry> span_free{};
  };

  struct DebugSnapshot {
    std::vector<LayerSnapshot> layers{};
  };

  DebugSnapshot snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    DebugSnapshot snap;
    snap.layers.reserve(layers_.size());

    for (const auto &layer : layers_) {
      snap.layers.push_back(createLayerSnapshot(layer));
    }
    return snap;
  }

  void validate() const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (std::size_t layer_index = 0; layer_index < layers_.size();
         ++layer_index) {
      validateLayer(layer_index);
    }
  }
#else
  using DebugSpanFreeEntry = uint32_t;
#endif

private:
  // ========================================================================
  // Constants
  // ========================================================================
  static constexpr uint32_t kNoParent = UINT32_MAX;
  static constexpr uint32_t kNoChild = UINT32_MAX;
  static constexpr uint32_t kInvalidLayer = UINT32_MAX;
  static constexpr std::size_t kSystemMinThreshold = alignof(double);

  static constexpr bool kDebugEnabled =
#if ORTEAF_CORE_DEBUG_ENABLED
      true;
#else
      false;
#endif

  // ========================================================================
  // Internal types
  // ========================================================================
  using SpanFreeTraits = detail::SpanFreeEntryTraits<kDebugEnabled>;
  using SpanFreeEntry = typename SpanFreeTraits::Type;
  using Slot = detail::SlotBase<kDebugEnabled, HeapRegion, BufferView>;
  using ChildCountAccessor = detail::SlotChildCountAccessor<kDebugEnabled>;

  struct Layer {
    explicit Layer(std::size_t size) : chunk_size(size) {}
    std::size_t chunk_size{0};
    ::orteaf::internal::base::HeapVector<Slot> slots;
    ::orteaf::internal::base::HeapVector<uint32_t> free_list;
    ::orteaf::internal::base::HeapVector<SpanFreeEntry> span_free;
  };

  // ========================================================================
  // Slot helpers
  // ========================================================================
  static State getState(const Slot &slot) noexcept {
    return static_cast<State>(slot.state);
  }

  static void setState(Slot &slot, State state) noexcept {
    slot.state = static_cast<uint8_t>(state);
  }

  static void resetSlot(Slot &slot) noexcept {
    slot.mapped_flag = false;
    slot.mapped = {};
    setState(slot, State::Free);
  }

  Slot createFreeSlot(HeapRegion region,
                      uint32_t parent_slot = kNoParent) const {
    Slot slot{};
    slot.region = region;
    slot.mapped = {};
    slot.mapped_flag = false;
    setState(slot, State::Free);
    slot.parent_slot = parent_slot;
    slot.child_layer = kNoChild;
    slot.child_begin = 0;
    ChildCountAccessor::set(slot, 0);
    return slot;
  }

  void markAsSplit(Slot &slot, uint32_t child_layer, uint32_t child_begin,
                   uint32_t child_count) {
    setState(slot, State::Split);
    slot.child_layer = child_layer;
    slot.child_begin = child_begin;
    ChildCountAccessor::set(slot, child_count);
  }

  // ========================================================================
  // Configuration helpers
  // ========================================================================
  static std::size_t computeInitialBytes(const Config &config) noexcept {
    if (config.initial_bytes != 0) {
      return config.initial_bytes;
    }
    if (config.levels.empty()) {
      return 0;
    }
    return config.levels[0];
  }

  static void validateLevels(const std::vector<std::size_t> &levels,
                             std::size_t threshold) {
    // levels が空の場合は initial_bytes=0 との組み合わせで許可される
    if (levels.empty()) {
      return;
    }

    // threshold が指定されている場合のみ追加チェックを行う
    if (threshold != 0) {
      ORTEAF_THROW_IF(threshold < kSystemMinThreshold, InvalidParameter,
                      "threshold must be >= system minimum");
      ORTEAF_THROW_UNLESS(::orteaf::internal::base::isPowerOfTwo(threshold),
                          InvalidParameter, "threshold must be power of two");
    }

    for (std::size_t i = 0; i < levels.size(); ++i) {
      ORTEAF_THROW_IF(levels[i] == 0, InvalidParameter,
                      "levels must be non-zero");

      if (i == 0)
        continue;

      ORTEAF_THROW_IF(levels[i - 1] < levels[i], InvalidParameter,
                      "levels must be non-increasing");
      ORTEAF_THROW_IF(levels[i - 1] % levels[i] != 0, InvalidParameter,
                      "adjacent levels must be divisible");

      // threshold が指定されている場合のみ、2の冪乗と割り切れチェックを行う
      if (threshold != 0 && levels[i] < threshold) {
        ORTEAF_THROW_UNLESS(::orteaf::internal::base::isPowerOfTwo(levels[i]),
                            InvalidParameter,
                            "levels below threshold must be power of two");
      }
      if (threshold != 0 && levels[i] > threshold) {
        ORTEAF_THROW_IF(
            levels[i] % threshold != 0, InvalidParameter,
            "levels above threshold must be divisible by threshold");
      }
    }
  }

  // ========================================================================
  // Layer selection
  // ========================================================================
  uint32_t pickLayer(std::size_t requested_size) const noexcept {
    uint32_t best = kInvalidLayer;
    for (uint32_t i = 0; i < layers_.size(); ++i) {
      if (requested_size <= layers_[i].chunk_size) {
        best = i; // levels は大きい順なので、より小さい層でも収まるか確認
      } else {
        break; // これより小さい層には入らない
      }
    }
    return best;
  }

  // ========================================================================
  // Free list operations
  // ========================================================================
  static uint32_t
  popFree(::orteaf::internal::base::HeapVector<uint32_t> &free_list) {
    const std::size_t last_index = free_list.size() - 1;
    const uint32_t value = free_list[last_index];
    free_list.resize(last_index);
    return value;
  }

  // ========================================================================
  // Region management
  // ========================================================================
  void addRegion(std::size_t bytes) {
    ORTEAF_THROW_IF(layers_.empty(), InvalidState, "No layers configured");

    auto &root_layer = layers_[0];
    const std::size_t chunk_size = root_layer.chunk_size;
    std::size_t remaining = (bytes == 0) ? chunk_size : bytes;

    HeapRegion base_region = heap_ops_->reserve(remaining);
    std::size_t offset = 0;

    while (remaining > 0) {
      const std::size_t step =
          (remaining >= chunk_size) ? chunk_size : remaining;
      const std::size_t slot_index = root_layer.slots.size();

      HeapRegion region{
          static_cast<void *>(static_cast<char *>(base_region.data()) + offset),
          step};

      root_layer.slots.emplaceBack(createFreeSlot(region));
      root_layer.free_list.pushBack(static_cast<uint32_t>(slot_index));

      offset += step;
      remaining -= step;
    }
  }

  // ========================================================================
  // Slot allocation
  // ========================================================================
  void ensureFreeSlot(uint32_t target_layer) {
    ORTEAF_THROW_IF(target_layer >= layers_.size(), OutOfRange,
                    "Layer index out of range");

    if (!layers_[target_layer].free_list.empty()) {
      return;
    }

    // 上位層を遡って空きを探す
    int parent_layer = findAvailableParentLayer(target_layer);

    // ルートにも無ければ新規リージョンを追加
    if (parent_layer < 0) {
      const std::size_t multiplier =
          (config_.region_multiplier == 0) ? 1 : config_.region_multiplier;
      addRegion(layers_[0].chunk_size * multiplier);
      parent_layer = 0;
    }

    // 見つかった親から target_layer まで段階的に split
    splitDownTo(static_cast<uint32_t>(parent_layer), target_layer);

    ORTEAF_THROW_IF(layers_[target_layer].free_list.empty(), OutOfMemory,
                    "Failed to ensure free slot");
  }

  int findAvailableParentLayer(uint32_t target_layer) const noexcept {
    for (int layer = static_cast<int>(target_layer) - 1; layer >= 0; --layer) {
      if (!layers_[layer].free_list.empty()) {
        return layer;
      }
    }
    return -1;
  }

  void splitDownTo(uint32_t from_layer, uint32_t to_layer) {
    for (uint32_t layer = from_layer; layer < to_layer; ++layer) {
      const uint32_t child_layer = layer + 1;
      if (!layers_[child_layer].free_list.empty()) {
        continue;
      }
      ORTEAF_THROW_IF(layers_[layer].free_list.empty(), OutOfMemory,
                      "Failed to refill parent layer");
      splitOne(layer, child_layer);
    }
  }

  // ========================================================================
  // Split / Merge operations
  // ========================================================================
  void splitOne(uint32_t parent_layer_index, uint32_t child_layer_index) {
    auto &parent_layer = layers_[parent_layer_index];
    auto &child_layer = layers_[child_layer_index];

    if (parent_layer.free_list.empty())
      return;

    const uint32_t parent_slot_index = popFree(parent_layer.free_list);
    Slot &parent_slot = parent_layer.slots[parent_slot_index];

    const std::size_t parent_size = parent_layer.chunk_size;
    const std::size_t child_size = child_layer.chunk_size;
    const std::size_t child_count = validateSplitSizes(parent_size, child_size);

    const uint32_t base_slot = acquireChildSlotRange(child_layer, child_count);
    initializeChildSlots(child_layer, base_slot, child_count, parent_slot,
                         parent_slot_index, child_size);
    markAsSplit(parent_slot, child_layer_index, base_slot,
                static_cast<uint32_t>(child_count));
  }

  static std::size_t validateSplitSizes(std::size_t parent_size,
                                        std::size_t child_size) {
    ORTEAF_THROW_IF(child_size == 0 || parent_size % child_size != 0,
                    InvalidParameter, "Non-divisible layer sizes");
    const std::size_t count = parent_size / child_size;
    ORTEAF_DEBUG_THROW_IF(count == 0, InvalidParameter, "Split count is zero");
    return count;
  }

  uint32_t acquireChildSlotRange(Layer &child_layer, std::size_t count) {
    // 連続ブロックの再利用を優先
    if (auto reused = tryReuseSpanFree(child_layer, count)) {
      return *reused;
    }
    // 新規確保
    const std::size_t base = child_layer.slots.size();
    child_layer.slots.resize(base + count);
    return static_cast<uint32_t>(base);
  }

  std::optional<uint32_t> tryReuseSpanFree(Layer &child_layer,
                                           std::size_t count) {
    for (std::size_t i = 0; i < child_layer.span_free.size(); ++i) {
      const auto &entry = child_layer.span_free[i];
      if (SpanFreeTraits::matchesCount(entry, static_cast<uint32_t>(count))) {
        const uint32_t base = SpanFreeTraits::begin(entry);
        // swap-and-pop で削除
        child_layer.span_free[i] = child_layer.span_free.back();
        child_layer.span_free.resize(child_layer.span_free.size() - 1);
        return base;
      }
    }
    return std::nullopt;
  }

  void initializeChildSlots(Layer &child_layer, uint32_t base_slot,
                            std::size_t count, const Slot &parent_slot,
                            uint32_t parent_slot_index,
                            std::size_t child_size) {
    for (std::size_t i = 0; i < count; ++i) {
      const std::size_t offset = i * child_size;
      HeapRegion region{
          static_cast<void *>(static_cast<char *>(parent_slot.region.data()) +
                              offset),
          child_size};

      Slot &child_slot = child_layer.slots[base_slot + i];
      child_slot = createFreeSlot(region, parent_slot_index);
      child_layer.free_list.pushBack(static_cast<uint32_t>(base_slot + i));
    }
  }

  void tryMergeParent(uint32_t child_layer_index, uint32_t parent_layer_index,
                      uint32_t parent_slot_index) {
    auto &parent_layer = layers_[parent_layer_index];
    if (parent_slot_index >= parent_layer.slots.size())
      return;

    Slot &parent_slot = parent_layer.slots[parent_slot_index];
    if (getState(parent_slot) != State::Split ||
        parent_slot.child_layer != child_layer_index) {
      return;
    }

    auto &child_layer = layers_[child_layer_index];
    const uint32_t child_begin = parent_slot.child_begin;
    const uint32_t child_count = ChildCountAccessor::get(
        parent_slot, parent_layer.chunk_size, child_layer.chunk_size);

    ORTEAF_DEBUG_THROW_IF(child_count !=
                              parent_layer.chunk_size / child_layer.chunk_size,
                          InvalidState, "Child count mismatch on merge");

    // すべての子が Free かチェック
    if (!allChildrenFree(child_layer, child_begin, child_count)) {
      return;
    }

    // 子範囲を再利用可能な span として戻す
    child_layer.span_free.pushBack(
        SpanFreeTraits::make(child_begin, child_count));

    // 親を Free に戻す
    setState(parent_slot, State::Free);
    parent_slot.child_layer = kNoChild;
    parent_slot.child_begin = 0;
    ChildCountAccessor::set(parent_slot, 0);
    parent_layer.free_list.pushBack(parent_slot_index);
  }

  bool allChildrenFree(const Layer &child_layer, uint32_t begin,
                       uint32_t count) const noexcept {
    for (uint32_t i = 0; i < count; ++i) {
      const uint32_t index = begin + i;
      if (index >= child_layer.slots.size())
        return false;
      if (getState(child_layer.slots[index]) != State::Free)
        return false;
    }
    return true;
  }

  // ========================================================================
  // Slot lookup
  // ========================================================================

  /// アドレスからスロットを特定する。{layer_index, slot_index}を返す。
  std::pair<uint32_t, uint32_t> findSlotByAddress(void *addr) const {
    for (uint32_t layer_idx = 0; layer_idx < layers_.size(); ++layer_idx) {
      const auto &layer = layers_[layer_idx];
      for (uint32_t slot_idx = 0; slot_idx < layer.slots.size(); ++slot_idx) {
        const Slot &slot = layer.slots[slot_idx];
        if (getState(slot) == State::InUse && slot.mapped.data() == addr) {
          return {layer_idx, slot_idx};
        }
      }
    }
    return {kInvalidLayer, 0};
  }

  // ========================================================================
  // Debug validation
  // ========================================================================
#if ORTEAF_CORE_DEBUG_ENABLED
  LayerSnapshot createLayerSnapshot(const Layer &layer) const {
    LayerSnapshot snapshot;
    snapshot.chunk_size = layer.chunk_size;

    snapshot.free_list.reserve(layer.free_list.size());
    for (std::size_t i = 0; i < layer.free_list.size(); ++i) {
      snapshot.free_list.push_back(layer.free_list[i]);
    }

    snapshot.span_free.reserve(layer.span_free.size());
    for (std::size_t i = 0; i < layer.span_free.size(); ++i) {
      const auto &entry = layer.span_free[i];
      snapshot.span_free.emplace_back(
          SpanFreeTraits::begin(entry),
          SpanFreeTraits::count(entry, 0, 1) // デバッグ時は count を直接取得
      );
    }

    snapshot.slots.reserve(layer.slots.size());
    for (std::size_t i = 0; i < layer.slots.size(); ++i) {
      const auto &slot = layer.slots[i];
      snapshot.slots.push_back(SlotSnapshot{
          getState(slot),
          slot.mapped_flag,
          slot.parent_slot,
          slot.child_layer,
          slot.child_begin,
          ChildCountAccessor::get(slot, 0, 0),
      });
    }
    return snapshot;
  }

  void validateLayer(std::size_t layer_index) const {
    const auto &layer = layers_[layer_index];
    validateFreeList(layer);
    validateSpanFree(layer);
    validateSplitSlots(layer_index);
  }

  void validateFreeList(const Layer &layer) const {
    std::vector<uint8_t> seen(layer.slots.size(), 0);
    for (std::size_t i = 0; i < layer.free_list.size(); ++i) {
      const auto index = layer.free_list[i];
      ORTEAF_THROW_IF(index >= layer.slots.size(), InvalidState,
                      "free_list index out of range");
      ORTEAF_THROW_IF(seen[index], InvalidState, "free_list duplicate");
      seen[index] = 1;
      ORTEAF_THROW_IF(getState(layer.slots[index]) != State::Free, InvalidState,
                      "free_list slot not free");
    }
  }

  void validateSpanFree(const Layer &layer) const {
    for (std::size_t i = 0; i < layer.span_free.size(); ++i) {
      const auto &entry = layer.span_free[i];
      const uint32_t begin = SpanFreeTraits::begin(entry);
      const uint32_t count = SpanFreeTraits::count(entry, 0, 1);
      ORTEAF_THROW_IF(begin >= layer.slots.size() ||
                          begin + count > layer.slots.size(),
                      InvalidState, "span_free out of range");
    }
  }

  void validateSplitSlots(std::size_t layer_index) const {
    const auto &layer = layers_[layer_index];
    for (std::size_t slot_index = 0; slot_index < layer.slots.size();
         ++slot_index) {
      const Slot &slot = layer.slots[slot_index];
      if (getState(slot) != State::Split)
        continue;

      ORTEAF_THROW_IF(slot.child_layer == kNoChild ||
                          slot.child_layer >= layers_.size(),
                      InvalidState, "split slot missing child layer");

      const auto &child_layer = layers_[slot.child_layer];
      const std::size_t expected_count =
          layer.chunk_size / child_layer.chunk_size;

      ORTEAF_THROW_IF(slot.child_begin >= child_layer.slots.size() ||
                          slot.child_begin + expected_count >
                              child_layer.slots.size(),
                      InvalidState, "split child range out of bounds");

      for (std::size_t i = 0; i < expected_count; ++i) {
        ORTEAF_THROW_IF(child_layer.slots[slot.child_begin + i].parent_slot !=
                            slot_index,
                        InvalidState, "child parent mismatch");
      }
    }
  }
#endif

  // ========================================================================
  // Member variables
  // ========================================================================
  Config config_{};
  HeapOps *heap_ops_{nullptr}; // 非所有
  std::vector<Layer> layers_{};
  mutable std::mutex mutex_{};
};

} // namespace orteaf::internal::execution::allocator::policies

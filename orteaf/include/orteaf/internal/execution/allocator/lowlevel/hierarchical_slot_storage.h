#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/base/math_utils.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/diagnostics/error/error_macros.h"
#include "orteaf/internal/execution/base/backend_traits.h"

namespace orteaf::internal::execution::allocator::policies {

/**
 * @brief 階層的スロットの状態管理用ストレージ
 */
template <class HeapOps, ::orteaf::internal::backend::Backend B>
class HierarchicalSlotStorage {
public:
  using BufferView =
      typename ::orteaf::internal::execution::base::BackendTraits<B>::BufferView;
  using HeapRegion =
      typename ::orteaf::internal::execution::base::BackendTraits<B>::HeapRegion;

  static constexpr uint32_t kNoParent = UINT32_MAX;
  static constexpr uint32_t kInvalidLayer = UINT32_MAX;
  static constexpr std::size_t kSystemMinThreshold = alignof(double);

  enum class State : uint8_t { Free, InUse, Split };

  struct Slot {
    HeapRegion region{};
    State state{State::Free};
    uint32_t parent_slot{kNoParent};
    uint32_t child_begin{0};
  };

  struct Layer {
    explicit Layer(std::size_t size) : slot_size(size) {}
    std::size_t slot_size{0};
    ::orteaf::internal::base::HeapVector<Slot> slots;
    ::orteaf::internal::base::HeapVector<uint32_t> free_list;
    ::orteaf::internal::base::HeapVector<uint32_t>
        span_free_list; // split時のbegin再利用用
  };

  struct Config {
    std::vector<std::size_t> levels; // 大きい順: {1MB, 256KB, 64KB}
    std::size_t initial_bytes{0};
    std::size_t expand_bytes{0};
    std::size_t threshold{0};
  };

  void initialize(const Config &config, HeapOps *heap_ops) {
    config_ = config;
    heap_ops_ = heap_ops;

    ORTEAF_THROW_IF_NULL(heap_ops_,
                         "HierarchicalSlotStorage requires non-null HeapOps*");
    ORTEAF_THROW_IF(config.levels.empty(), InvalidParameter,
                    "levels must not be empty");
    validateLevels(config.levels, config.threshold);
    ORTEAF_THROW_IF(config.initial_bytes > 0 &&
                        (config.initial_bytes % config.levels[0]) != 0,
                    InvalidParameter,
                    "initial_bytes must be a multiple of levels[0]");
    ORTEAF_THROW_IF(config.expand_bytes > 0 &&
                        (config.expand_bytes % config.levels[0]) != 0,
                    InvalidParameter,
                    "expand_bytes must be a multiple of levels[0]");

    layers_.clear();
    for (auto size : config.levels) {
      layers_.emplace_back(size);
    }

    std::size_t initial = config.initial_bytes;
    if (initial == 0) {
      initial = config.levels[0];
    }
    addRegion(initial);
  }

  // ========================================================================
  // Accessors
  // ========================================================================

  [[nodiscard]] const Config &config() const noexcept { return config_; }
  [[nodiscard]] HeapOps *heapOps() const noexcept { return heap_ops_; }
  [[nodiscard]] std::vector<Layer> &layers() noexcept { return layers_; }
  [[nodiscard]] const std::vector<Layer> &layers() const noexcept {
    return layers_;
  }
  [[nodiscard]] std::mutex &mutex() noexcept { return mutex_; }

#if ORTEAF_ENABLE_TEST
  struct DebugSlot {
    State state;
    uint32_t parent_slot;
    uint32_t child_begin;
  };
  struct DebugLayer {
    std::size_t slot_size;
    std::vector<DebugSlot> slots;
    std::vector<uint32_t> free_list;
    std::vector<uint32_t> span_free_list;
  };

  // テスト用: 指定レイヤのスロット情報を参照
  [[nodiscard]] const Layer &debugLayer(uint32_t layer_idx) const {
    return layers_.at(layer_idx);
  }

  // テスト用: すべてのレイヤの状態スナップショットを取得
  [[nodiscard]] std::vector<DebugLayer> debugSnapshot() const {
    std::vector<DebugLayer> out;
    out.reserve(layers_.size());
    for (const auto &L : layers_) {
      DebugLayer dl;
      dl.slot_size = L.slot_size;
      dl.slots.reserve(L.slots.size());
      for (std::size_t i = 0; i < L.slots.size(); ++i) {
        const auto &s = L.slots[i];
        dl.slots.push_back(DebugSlot{s.state, s.parent_slot, s.child_begin});
      }
      dl.free_list.reserve(L.free_list.size());
      for (std::size_t i = 0; i < L.free_list.size(); ++i) {
        dl.free_list.push_back(L.free_list[i]);
      }
      dl.span_free_list.reserve(L.span_free_list.size());
      for (std::size_t i = 0; i < L.span_free_list.size(); ++i) {
        dl.span_free_list.push_back(L.span_free_list[i]);
      }
      out.emplace_back(std::move(dl));
    }
    return out;
  }
#endif

  // ========================================================================
  // Pure functions (static)
  // ========================================================================

  static uint32_t pickLayer(const std::vector<std::size_t> &levels,
                            std::size_t size) noexcept {
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

  static void validateLevels(const std::vector<std::size_t> &levels,
                             std::size_t threshold) {
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

  static bool hasFreeSlot(const Layer &layer) noexcept {
    return !layer.free_list.empty();
  }

  static uint32_t popFreeSlot(Layer &layer) {
    const auto last = layer.free_list.size() - 1;
    const auto value = layer.free_list[last];
    layer.free_list.resize(last);
    return value;
  }

  static void markSlotInUse(Slot &slot) noexcept { slot.state = State::InUse; }

  static void markSlotFree(Slot &slot) noexcept { slot.state = State::Free; }

  static void markSlotSplit(Slot &slot, uint32_t child_begin) noexcept {
    slot.state = State::Split;
    slot.child_begin = child_begin;
  }

  static bool allSiblingsFree(const Layer &child_layer, uint32_t begin,
                              std::size_t count) noexcept {
    for (std::size_t i = 0; i < count; ++i) {
      if (child_layer.slots[begin + i].state != State::Free) {
        return false;
      }
    }
    return true;
  }

  static void splitSlot(Layer &parent, Layer &child, uint32_t parent_slot_idx) {
    Slot &parent_slot = parent.slots[parent_slot_idx];
    const std::size_t count = parent.slot_size / child.slot_size;

    uint32_t child_begin;

    // span_free_listから再利用を試みる
    if (!child.span_free_list.empty()) {
      const auto last = child.span_free_list.size() - 1;
      child_begin = child.span_free_list[last];
      child.span_free_list.resize(last);

      // 既存のスロットを再初期化
      for (std::size_t i = 0; i < count; ++i) {
        Slot &slot = child.slots[child_begin + i];
        slot.region = HeapRegion{
            static_cast<void *>(static_cast<char *>(parent_slot.region.data()) +
                                i * child.slot_size),
            child.slot_size};
        slot.state = State::Free;
        slot.parent_slot = parent_slot_idx;
        slot.child_begin = 0;

        child.free_list.pushBack(static_cast<uint32_t>(child_begin + i));
      }
    } else {
      // 新規作成
      child_begin = static_cast<uint32_t>(child.slots.size());

      for (std::size_t i = 0; i < count; ++i) {
        Slot new_slot{};
        new_slot.region = HeapRegion{
            static_cast<void *>(static_cast<char *>(parent_slot.region.data()) +
                                i * child.slot_size),
            child.slot_size};
        new_slot.state = State::Free;
        new_slot.parent_slot = parent_slot_idx;

        child.slots.emplaceBack(new_slot);
        child.free_list.pushBack(static_cast<uint32_t>(child_begin + i));
      }
    }

    markSlotSplit(parent_slot, child_begin);
  }

  [[nodiscard]] std::vector<uint32_t>
  computeRequestSlots(std::size_t size) const {
    const auto &levels = config_.levels;
    std::size_t b = levels.back();
    std::size_t N = (size + b - 1) / b;

    std::vector<uint32_t> rs(levels.size(), 0);

    for (std::size_t i = 0; i < levels.size() - 1; ++i) {
      std::size_t u = levels[i] / b;
      rs[i] = static_cast<uint32_t>(N / u);
      N -= rs[i] * u;
    }
    rs.back() = static_cast<uint32_t>(N);

    return rs;
  }

  // ========================================================================
  // Region management
  // ========================================================================

  void addRegion(std::size_t bytes) {
    ORTEAF_THROW_IF(layers_.empty(), InvalidState, "No layers configured");

    auto &root = layers_[0];
    std::size_t remaining = (bytes == 0) ? root.slot_size : bytes;

    HeapRegion base_region = heap_ops_->reserve(remaining);
    std::size_t offset = 0;

    while (remaining > 0) {
      const std::size_t step =
          (remaining >= root.slot_size) ? root.slot_size : remaining;
      const auto slot_index = static_cast<uint32_t>(root.slots.size());

      Slot slot{};
      slot.region = HeapRegion{
          static_cast<void *>(static_cast<char *>(base_region.data()) + offset),
          step};
      slot.state = State::Free;

      root.slots.emplaceBack(slot);
      root.free_list.pushBack(slot_index);

      offset += step;
      remaining -= step;
    }
  }

private:
  Config config_{};
  HeapOps *heap_ops_{nullptr};
  std::vector<Layer> layers_;
  mutable std::mutex mutex_;
};

} // namespace orteaf::internal::execution::allocator::policies

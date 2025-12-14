#pragma once

#include <atomic>
#include <cstdint>

#include <orteaf/internal/runtime/base/lease/category.h>
#include <orteaf/internal/runtime/base/lease/concepts.h>

namespace orteaf::internal::runtime::base {

/// @brief Weak-unique control block - single ownership with weak reference
/// support
/// @details Allows weak references to observe the resource without owning it.
/// The resource is destroyed when the strong owner releases, but control block
/// persists until all weak references are gone.
template <typename SlotT>
  requires SlotConcept<SlotT>
struct WeakUniqueControlBlock {
  // WeakUnique does not support generation tracking.
  // Weak references use isAlive()/tryPromote() for validity checking.
  static_assert(!SlotT::has_generation,
                "WeakUniqueControlBlock does not support generation tracking. "
                "Use a Slot without generation (e.g., Slot<T> or RawSlot<T>).");

  using Category = lease_category::WeakUnique;
  using Slot = SlotT;

  std::atomic<bool> in_use{false};
  std::atomic<std::uint32_t> weak_count{0};
  SlotT slot{};

  WeakUniqueControlBlock() = default;
  WeakUniqueControlBlock(const WeakUniqueControlBlock &) = delete;
  WeakUniqueControlBlock &operator=(const WeakUniqueControlBlock &) = delete;

  WeakUniqueControlBlock(WeakUniqueControlBlock &&other) noexcept
      : slot(std::move(other.slot)) {
    in_use.store(other.in_use.load(std::memory_order_relaxed),
                 std::memory_order_relaxed);
    weak_count.store(other.weak_count.load(std::memory_order_relaxed),
                     std::memory_order_relaxed);
  }

  WeakUniqueControlBlock &operator=(WeakUniqueControlBlock &&other) noexcept {
    if (this != &other) {
      in_use.store(other.in_use.load(std::memory_order_relaxed),
                   std::memory_order_relaxed);
      weak_count.store(other.weak_count.load(std::memory_order_relaxed),
                       std::memory_order_relaxed);
      slot = std::move(other.slot);
    }
    return *this;
  }

  /// @brief Try to acquire exclusive (strong) ownership
  /// @return true if successfully acquired
  bool tryAcquire() noexcept {
    bool expected = false;
    return in_use.compare_exchange_strong(
        expected, true, std::memory_order_acquire, std::memory_order_relaxed);
  }

  /// @brief Release strong ownership
  void release() noexcept { in_use.store(false, std::memory_order_release); }

  /// @brief Acquire a weak reference
  void acquireWeak() noexcept {
    weak_count.fetch_add(1, std::memory_order_relaxed);
  }

  /// @brief Release a weak reference
  /// @return true if this was the last weak reference and resource is not in
  /// use
  bool releaseWeak() noexcept {
    const auto prev = weak_count.fetch_sub(1, std::memory_order_acq_rel);
    return prev == 1 && !in_use.load(std::memory_order_acquire);
  }

  /// @brief Check if strong owner exists
  bool isAlive() const noexcept {
    return in_use.load(std::memory_order_acquire);
  }

  /// @brief Try to promote weak reference to strong
  /// @return true if successfully promoted (same as tryAcquire)
  bool tryPromote() noexcept { return tryAcquire(); }

  /// @brief Check if fully released (not in use)
  bool isReleased() const noexcept { return !isAlive(); }

  /// @brief Check if completely released (not in use and no weak references)
  bool isFullyReleased() const noexcept {
    return !in_use.load(std::memory_order_acquire) &&
           weak_count.load(std::memory_order_acquire) == 0;
  }

  /// @brief Prepare for reuse - validates state and increments generation
  /// @return true if successfully prepared (was released), false if still in
  /// use
  bool prepareForReuse() noexcept {
    if (!isReleased()) {
      return false; // Still in use, cannot reuse
    }
    if constexpr (SlotT::has_generation) {
      slot.incrementGeneration();
    }
    return true;
  }
};

// Verify concept satisfaction
static_assert(ControlBlockConcept<WeakUniqueControlBlock<int>>);
static_assert(WeakableControlBlockConcept<WeakUniqueControlBlock<int>>);
static_assert(PromotableControlBlockConcept<WeakUniqueControlBlock<int>>);

} // namespace orteaf::internal::runtime::base

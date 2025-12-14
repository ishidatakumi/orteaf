#pragma once

#include <atomic>
#include <cstdint>

#include <orteaf/internal/runtime/base/lease/category.h>
#include <orteaf/internal/runtime/base/lease/concepts.h>

namespace orteaf::internal::runtime::base {

/// @brief Weak-shared control block - shared ownership with weak reference
/// support
/// @details Like std::shared_ptr with std::weak_ptr support. Reference counted
/// with separate strong and weak counts.
template <typename SlotT>
  requires SlotConcept<SlotT>
struct WeakSharedControlBlock {
  using Category = lease_category::WeakShared;
  using Slot = SlotT;

  std::atomic<std::uint32_t> strong_count{0};
  std::atomic<std::uint32_t> weak_count{0};
  SlotT slot{};

  WeakSharedControlBlock() = default;
  WeakSharedControlBlock(const WeakSharedControlBlock &) = delete;
  WeakSharedControlBlock &operator=(const WeakSharedControlBlock &) = delete;

  WeakSharedControlBlock(WeakSharedControlBlock &&other) noexcept
      : slot(std::move(other.slot)) {
    strong_count.store(other.strong_count.load(std::memory_order_relaxed),
                       std::memory_order_relaxed);
    weak_count.store(other.weak_count.load(std::memory_order_relaxed),
                     std::memory_order_relaxed);
  }

  WeakSharedControlBlock &operator=(WeakSharedControlBlock &&other) noexcept {
    if (this != &other) {
      strong_count.store(other.strong_count.load(std::memory_order_relaxed),
                         std::memory_order_relaxed);
      weak_count.store(other.weak_count.load(std::memory_order_relaxed),
                       std::memory_order_relaxed);
      slot = std::move(other.slot);
    }
    return *this;
  }

  /// @brief Try to acquire first strong reference
  /// @return true if this is the first acquisition
  bool tryAcquire() noexcept {
    std::uint32_t expected = 0;
    return strong_count.compare_exchange_strong(
        expected, 1, std::memory_order_acquire, std::memory_order_relaxed);
  }

  /// @brief Acquire a strong reference (increment count)
  void acquire() noexcept {
    strong_count.fetch_add(1, std::memory_order_relaxed);
  }

  /// @brief Acquire a strong reference (alias for acquire)
  void acquireStrong() noexcept { acquire(); }

  /// @brief Release a strong reference
  /// @return true if this was the last strong reference
  bool release() noexcept {
    return strong_count.fetch_sub(1, std::memory_order_acq_rel) == 1;
  }

  /// @brief Release a strong reference (alias for release)
  bool releaseStrong() noexcept { return release(); }

  /// @brief Acquire a weak reference
  void acquireWeak() noexcept {
    weak_count.fetch_add(1, std::memory_order_relaxed);
  }

  /// @brief Release a weak reference
  /// @return true if this was the last reference (strong and weak both zero)
  bool releaseWeak() noexcept {
    const auto prev = weak_count.fetch_sub(1, std::memory_order_acq_rel);
    return prev == 1 && strong_count.load(std::memory_order_acquire) == 0;
  }

  /// @brief Try to promote weak reference to strong
  /// @return true if successfully promoted (strong count was > 0)
  bool tryPromote() noexcept {
    std::uint32_t current = strong_count.load(std::memory_order_acquire);
    while (current > 0) {
      if (strong_count.compare_exchange_weak(current, current + 1,
                                             std::memory_order_acquire,
                                             std::memory_order_relaxed)) {
        return true;
      }
    }
    return false;
  }

  /// @brief Get current strong reference count
  std::uint32_t count() const noexcept {
    return strong_count.load(std::memory_order_acquire);
  }

  /// @brief Check if any strong references exist
  bool isAlive() const noexcept { return count() > 0; }

  /// @brief Check if fully released (strong count == 0)
  bool isReleased() const noexcept { return count() == 0; }

  /// @brief Check if completely released (both strong and weak counts == 0)
  bool isFullyReleased() const noexcept {
    return strong_count.load(std::memory_order_acquire) == 0 &&
           weak_count.load(std::memory_order_acquire) == 0;
  }

  /// @brief Prepare for reuse - validates state and increments generation
  /// @return true if successfully prepared (was released), false if still in
  /// use
  bool prepareForReuse() noexcept {
    if (!isReleased()) {
      return false; // Still has strong references, cannot reuse
    }
    if constexpr (SlotT::has_generation) {
      slot.incrementGeneration();
    }
    return true;
  }
};

// Verify concept satisfaction
static_assert(ControlBlockConcept<WeakSharedControlBlock<int>>);
static_assert(SharedControlBlockConcept<WeakSharedControlBlock<int>>);
static_assert(WeakableControlBlockConcept<WeakSharedControlBlock<int>>);
static_assert(PromotableControlBlockConcept<WeakSharedControlBlock<int>>);

} // namespace orteaf::internal::runtime::base

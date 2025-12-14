#pragma once

#include <atomic>

#include <orteaf/internal/runtime/base/lease/category.h>
#include <orteaf/internal/runtime/base/lease/concepts.h>

namespace orteaf::internal::runtime::base {

/// @brief Unique control block - single ownership with in_use flag
/// @details Only one lease can hold this resource at a time.
/// Uses atomic CAS for thread-safe acquisition.
template <typename SlotT>
  requires SlotConcept<SlotT>
struct UniqueControlBlock {
  using Category = lease_category::Unique;
  using Slot = SlotT;

  std::atomic<bool> in_use{false};
  SlotT slot{};

  UniqueControlBlock() = default;
  UniqueControlBlock(const UniqueControlBlock &) = delete;
  UniqueControlBlock &operator=(const UniqueControlBlock &) = delete;

  UniqueControlBlock(UniqueControlBlock &&other) noexcept
      : slot(std::move(other.slot)) {
    in_use.store(other.in_use.load(std::memory_order_relaxed),
                 std::memory_order_relaxed);
  }

  UniqueControlBlock &operator=(UniqueControlBlock &&other) noexcept {
    if (this != &other) {
      in_use.store(other.in_use.load(std::memory_order_relaxed),
                   std::memory_order_relaxed);
      slot = std::move(other.slot);
    }
    return *this;
  }

  /// @brief Try to acquire exclusive ownership
  /// @return true if successfully acquired, false if already in use
  bool tryAcquire() noexcept {
    bool expected = false;
    return in_use.compare_exchange_strong(
        expected, true, std::memory_order_acquire, std::memory_order_relaxed);
  }

  /// @brief Release ownership
  void release() noexcept { in_use.store(false, std::memory_order_release); }

  /// @brief Check if currently in use
  bool isAlive() const noexcept {
    return in_use.load(std::memory_order_acquire);
  }

  /// @brief Check if fully released (not in use)
  bool isReleased() const noexcept { return !isAlive(); }

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
static_assert(ControlBlockConcept<UniqueControlBlock<int>>);

} // namespace orteaf::internal::runtime::base

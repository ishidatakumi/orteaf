#pragma once

#include <atomic>
#include <concepts>
#include <cstdint>
#include <utility>

#include <orteaf/internal/runtime/base/lease/control_block/shared.h>

namespace orteaf::internal::runtime::base {

/// @brief Lockable shared control block - shared ownership with exclusive lock
/// @details Extends SharedControlBlock with a lock flag.
///          Supports shared ownership (ref counting) and explicit locking.
///          canShutdown() returns true when count == 0 AND not locked.
template <typename SlotT>
  requires SlotConcept<SlotT>
class LockableSharedControlBlock : public SharedControlBlock<SlotT> {
public:
  using Base = SharedControlBlock<SlotT>;
  using Category =
      typename Base::Category; // Kept as Shared for now? Or new category?
  using Slot = typename Base::Slot;
  using Payload = typename Base::Payload;

  // Constructor / Assignment
  LockableSharedControlBlock() = default;
  LockableSharedControlBlock(const LockableSharedControlBlock &) = delete;
  LockableSharedControlBlock &
  operator=(const LockableSharedControlBlock &) = delete;

  LockableSharedControlBlock(LockableSharedControlBlock &&other) noexcept
      : Base(std::move(other)),
        locked_(other.locked_.load(std::memory_order_relaxed)) {}

  LockableSharedControlBlock &
  operator=(LockableSharedControlBlock &&other) noexcept {
    if (this != &other) {
      Base::operator=(std::move(other));
      locked_.store(other.locked_.load(std::memory_order_relaxed),
                    std::memory_order_relaxed);
    }
    return *this;
  }

  // =========================================================================
  // Locking API
  // =========================================================================

  /// @brief Try to acquire exclusive lock
  /// @return true if lock acquired, false if already locked
  bool tryLock() noexcept {
    bool expected = false;
    return locked_.compare_exchange_strong(
        expected, true, std::memory_order_acquire, std::memory_order_relaxed);
  }

  /// @brief Release exclusive lock
  void unlock() noexcept { locked_.store(false, std::memory_order_release); }

  /// @brief Check if currently locked
  bool isLocked() const noexcept {
    return locked_.load(std::memory_order_acquire);
  }

  // =========================================================================
  // Lifecycle Overrides
  // =========================================================================

  /// @brief Release a shared reference, ensuring lock is cleared if last ref
  /// @return true if this was the last reference
  bool release() noexcept {
    bool fullyReleased = Base::release();
    if (fullyReleased) {
      // If we are the last one out, ensure lock is cleared (sanitization)
      if (locked_.load(std::memory_order_acquire)) {
        locked_.store(false, std::memory_order_release);
      }
    }
    return fullyReleased;
  }

  /// @brief Check if shutdown is allowed
  /// @return true if strong count is 0 AND not locked
  bool canShutdown() const noexcept {
    return this->count() == 0 && !locked_.load(std::memory_order_acquire);
  }

private:
  std::atomic<bool> locked_{false};
};

} // namespace orteaf::internal::runtime::base

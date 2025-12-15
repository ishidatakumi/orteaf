#pragma once

#include <atomic>
#include <cstdint>

#include <orteaf/internal/runtime/base/lease/category.h>
#include <orteaf/internal/runtime/base/lease/concepts.h>
#include <orteaf/internal/runtime/base/lease/slot.h>

namespace orteaf::internal::runtime::base {

/// @brief Weak-unique control block - single ownership with weak reference
/// support
/// @details Allows weak references to observe the resource without owning it.
/// The resource is destroyed when the strong owner releases, but control block
/// persists until all weak references are gone.
template <typename SlotT>
  requires SlotConcept<SlotT>
class WeakUniqueControlBlock {
  // WeakUnique does not support generation tracking.
  static_assert(!SlotT::has_generation,
                "WeakUniqueControlBlock does not support generation tracking. "
                "Use a Slot without generation (e.g., Slot<T> or RawSlot<T>).");

public:
  using Category = lease_category::WeakUnique;
  using Slot = SlotT;
  using Payload = typename SlotT::Payload;

  WeakUniqueControlBlock() = default;
  WeakUniqueControlBlock(const WeakUniqueControlBlock &) = delete;
  WeakUniqueControlBlock &operator=(const WeakUniqueControlBlock &) = delete;

  WeakUniqueControlBlock(WeakUniqueControlBlock &&other) noexcept
      : slot_(std::move(other.slot_)) {
    in_use_.store(other.in_use_.load(std::memory_order_relaxed),
                  std::memory_order_relaxed);
    weak_count_.store(other.weak_count_.load(std::memory_order_relaxed),
                      std::memory_order_relaxed);
  }

  WeakUniqueControlBlock &operator=(WeakUniqueControlBlock &&other) noexcept {
    if (this != &other) {
      in_use_.store(other.in_use_.load(std::memory_order_relaxed),
                    std::memory_order_relaxed);
      weak_count_.store(other.weak_count_.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
      slot_ = std::move(other.slot_);
    }
    return *this;
  }

  // =========================================================================
  // Concept Required API
  // =========================================================================

  /// @brief Acquire exclusive ownership
  /// @return true if successfully acquired, false if already in use
  bool acquire() noexcept {
    bool expected = false;
    return in_use_.compare_exchange_strong(
        expected, true, std::memory_order_acquire, std::memory_order_relaxed);
  }

  /// @brief Release strong ownership
  /// @return true if was in use and now released, false if wasn't in use
  bool release() noexcept {
    bool expected = true;
    return in_use_.compare_exchange_strong(
        expected, false, std::memory_order_release, std::memory_order_relaxed);
  }

  /// @brief Check if strong owner exists
  bool isAlive() const noexcept {
    return in_use_.load(std::memory_order_acquire);
  }

  /// @brief Mark slot as initialized/valid
  void validate() noexcept {
    if constexpr (SlotT::has_initialized) {
      slot_.markInitialized();
    }
  }

  /// @brief Mark slot as uninitialized/invalid
  void invalidate() noexcept {
    if constexpr (SlotT::has_initialized) {
      slot_.markUninitialized();
    }
  }

  // =========================================================================
  // Weak Reference API (WeakableControlBlockConcept)
  // =========================================================================

  /// @brief Acquire a weak reference
  void acquireWeak() noexcept {
    weak_count_.fetch_add(1, std::memory_order_relaxed);
  }

  /// @brief Release a weak reference
  /// @return true if this was the last weak reference and resource is not in
  /// use
  bool releaseWeak() noexcept {
    const auto prev = weak_count_.fetch_sub(1, std::memory_order_acq_rel);
    return prev == 1 && !in_use_.load(std::memory_order_acquire);
  }

  /// @brief Try to promote weak reference to strong
  /// @return true if successfully promoted
  bool tryPromote() noexcept { return acquire(); }

  // =========================================================================
  // Payload Access
  // =========================================================================

  /// @brief Access the payload
  Payload &payload() noexcept { return slot_.get(); }
  const Payload &payload() const noexcept { return slot_.get(); }

  // =========================================================================
  // Additional Queries
  // =========================================================================

  /// @brief Check if slot is initialized
  bool isInitialized() const noexcept { return slot_.isInitialized(); }

  /// @brief Get weak reference count
  std::uint32_t weakCount() const noexcept {
    return weak_count_.load(std::memory_order_acquire);
  }

private:
  std::atomic<bool> in_use_{false};
  std::atomic<std::uint32_t> weak_count_{0};
  SlotT slot_{};
};

// Verify concept satisfaction
static_assert(ControlBlockConcept<WeakUniqueControlBlock<Slot<int>>>);
static_assert(WeakableControlBlockConcept<WeakUniqueControlBlock<Slot<int>>>);
static_assert(PromotableControlBlockConcept<WeakUniqueControlBlock<Slot<int>>>);

} // namespace orteaf::internal::runtime::base

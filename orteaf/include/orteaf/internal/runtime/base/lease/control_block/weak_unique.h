#pragma once

#include <atomic>
#include <concepts>
#include <cstdint>
#include <utility>

#include <orteaf/internal/runtime/base/lease/category.h>
#include <orteaf/internal/runtime/base/lease/slot.h>

namespace orteaf::internal::runtime::base {

/// @brief Weak-unique control block - single ownership with weak reference
/// support
/// @details Allows weak references to observe the resource without owning it.
/// The resource is destroyed when the strong owner releases, but control block
/// persists until all weak references are gone.
/// isAlive() returns the in_use state.
template <typename SlotT>
  requires SlotConcept<SlotT>
class WeakUniqueControlBlock {
  // WeakUnique does not support generation tracking.
  static_assert(!SlotT::has_generation,
                "WeakUniqueControlBlock does not support generation tracking. "
                "Use RawSlot<T> instead.");

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
  // Lifecycle API
  // =========================================================================

  /// @brief Acquire exclusive ownership, creating resource if needed
  /// @tparam CreateFn Callable that takes Payload& and returns bool
  /// @return true if acquired and created, false if already in use or creation
  /// failed
  template <typename CreateFn>
    requires std::invocable<CreateFn, Payload &> &&
             std::convertible_to<std::invoke_result_t<CreateFn, Payload &>,
                                 bool>
  bool acquire(CreateFn &&createFn) noexcept {
    bool expected = false;
    if (!in_use_.compare_exchange_strong(expected, true,
                                         std::memory_order_acquire,
                                         std::memory_order_relaxed)) {
      return false; // Already in use
    }

    // Try to create the resource
    if (!slot_.create(std::forward<CreateFn>(createFn))) {
      // Creation failed, release ownership
      in_use_.store(false, std::memory_order_release);
      return false;
    }

    return true;
  }

  /// @brief Release strong ownership (for reuse)
  /// @return true if was in use and now released, false if wasn't in use
  bool release() noexcept {
    bool expected = true;
    if (in_use_.compare_exchange_strong(expected, false,
                                        std::memory_order_release,
                                        std::memory_order_relaxed)) {
      return true;
    }
    return false;
  }

  /// @brief Release and destroy the resource (non-reusable)
  /// @tparam DestroyFn Callable that takes Payload&
  /// @return true if released and destroyed, false otherwise
  template <typename DestroyFn>
    requires std::invocable<DestroyFn, Payload &>
  bool releaseAndDestroy(DestroyFn &&destroyFn) {
    bool expected = true;
    if (in_use_.compare_exchange_strong(expected, false,
                                        std::memory_order_release,
                                        std::memory_order_relaxed)) {
      return slot_.destroy(std::forward<DestroyFn>(destroyFn));
    }
    return false;
  }

  /// @brief Check if resource is currently acquired
  bool isAlive() const noexcept {
    return in_use_.load(std::memory_order_acquire);
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
  bool tryPromote() noexcept {
    bool expected = false;
    return in_use_.compare_exchange_strong(
        expected, true, std::memory_order_acquire, std::memory_order_relaxed);
  }

  // =========================================================================
  // Payload Access
  // =========================================================================

  /// @brief Access the payload
  Payload &payload() noexcept { return slot_.get(); }
  const Payload &payload() const noexcept { return slot_.get(); }

  // =========================================================================
  // Additional Queries
  // =========================================================================

  /// @brief Get weak reference count
  std::uint32_t weakCount() const noexcept {
    return weak_count_.load(std::memory_order_acquire);
  }

  // =========================================================================
  // Creation State (delegated to Slot)
  // =========================================================================

  /// @brief Check if resource has been created
  bool isCreated() const noexcept { return slot_.isCreated(); }

  /// @brief Create the resource by executing the factory
  template <typename Factory>
    requires std::invocable<Factory, Payload &>
  auto create(Factory &&factory) -> decltype(auto) {
    return slot_.create(std::forward<Factory>(factory));
  }

  /// @brief Destroy the resource by executing the destructor
  template <typename Destructor>
    requires std::invocable<Destructor, Payload &>
  void destroy(Destructor &&destructor) {
    slot_.destroy(std::forward<Destructor>(destructor));
  }

private:
  std::atomic<bool> in_use_{false};
  std::atomic<std::uint32_t> weak_count_{0};
  SlotT slot_{};
};

} // namespace orteaf::internal::runtime::base

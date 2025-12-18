#pragma once

#include <atomic>
#include <concepts>
#include <cstdint>
#include <utility>

#include <orteaf/internal/runtime/base/lease/category.h>
#include <orteaf/internal/runtime/base/lease/slot.h>

namespace orteaf::internal::runtime::base {

/// @brief Weak-shared control block - shared ownership with weak reference
/// support
/// @details Like std::shared_ptr with std::weak_ptr support. Reference counted
/// with separate strong and weak counts.
/// canTeardown() returns true when count == 0.
template <typename SlotT>
  requires SlotConcept<SlotT>
class WeakSharedControlBlock {
public:
  using Category = lease_category::WeakShared;
  using Slot = SlotT;
  using Payload = typename SlotT::Payload;

  WeakSharedControlBlock() = default;
  WeakSharedControlBlock(const WeakSharedControlBlock &) = delete;
  WeakSharedControlBlock &operator=(const WeakSharedControlBlock &) = delete;

  WeakSharedControlBlock(WeakSharedControlBlock &&other) noexcept
      : slot_(std::move(other.slot_)) {
    strong_count_.store(other.strong_count_.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
    weak_count_.store(other.weak_count_.load(std::memory_order_relaxed),
                      std::memory_order_relaxed);
  }

  WeakSharedControlBlock &operator=(WeakSharedControlBlock &&other) noexcept {
    if (this != &other) {
      strong_count_.store(other.strong_count_.load(std::memory_order_relaxed),
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

  /// @brief Acquire a strong reference, creating resource if needed
  /// @tparam CreateFn Callable that takes Payload& and returns bool
  /// @return true if acquired and created, false if creation failed
  template <typename CreateFn>
    requires std::invocable<CreateFn, Payload &> &&
             std::convertible_to<std::invoke_result_t<CreateFn, Payload &>,
                                 bool>
  bool acquire(CreateFn &&createFn) noexcept {
    // Try to create the resource
    if (!slot_.create(std::forward<CreateFn>(createFn))) {
      return false; // Creation failed
    }

    // Increment reference count
    strong_count_.fetch_add(1, std::memory_order_relaxed);
    return true;
  }

  /// @brief Release a strong reference (for reuse)
  /// @return true if this was the last strong reference, false otherwise.
  /// @note Generation is incremented when last strong ref released (weak refs
  /// don't block).
  bool release() noexcept {
    if (strong_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      // Last strong reference released - increment generation immediately
      // (weak references don't prevent generation increment)
      if constexpr (SlotT::has_generation) {
        slot_.incrementGeneration();
      }
      if (weak_count_.load(std::memory_order_acquire) == 0) {
        return true;
      }
    }
    return false;
  }

  /// @brief Release and destroy the resource (non-reusable)
  /// @tparam DestroyFn Callable that takes Payload&
  /// @return true if this was the last strong reference and destroyed.
  /// @note Generation is incremented when last strong ref released (weak refs
  /// don't block).
  template <typename DestroyFn>
    requires std::invocable<DestroyFn, Payload &>
  bool releaseAndDestroy(DestroyFn &&destroyFn) {
    if (strong_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      // Last strong reference - destroy the resource
      slot_.destroy(std::forward<DestroyFn>(destroyFn));

      // Increment generation immediately when last strong is released
      // (weak references don't prevent generation increment)
      if constexpr (SlotT::has_generation) {
        slot_.incrementGeneration();
      }
      if (weak_count_.load(std::memory_order_acquire) == 0) {
        return true;
      }
    }
    return false;
  }

  /// @brief Check if teardown is allowed
  /// @return true if no strong references (count == 0)
  bool canTeardown() const noexcept { return count() == 0; }

  /// @brief Check if shutdown is allowed
  /// @return true if strong count 0 AND weak count 0
  bool canShutdown() const noexcept { return count() == 0 && weakCount() == 0; }

  // =========================================================================
  // Shared-specific API (SharedControlBlockConcept)
  // =========================================================================

  /// @brief Get current strong reference count
  std::uint32_t count() const noexcept {
    return strong_count_.load(std::memory_order_acquire);
  }

  // =========================================================================
  // Weak Reference API (WeakableControlBlockConcept)
  // =========================================================================

  /// @brief Acquire a weak reference
  void acquireWeak() noexcept {
    weak_count_.fetch_add(1, std::memory_order_relaxed);
  }

  /// @brief Release a weak reference
  /// @return true if this was the last weak reference AND strong count is zero.
  /// @note Generation is NOT updated here - it's updated when strong ref is
  /// released.
  bool releaseWeak() noexcept {
    weak_count_.fetch_sub(1, std::memory_order_acq_rel);
    return weak_count_.load(std::memory_order_acquire) == 0 &&
           strong_count_.load(std::memory_order_acquire) == 0;
  }

  /// @brief Try to promote weak reference to strong
  /// @return true if successfully promoted (resource is created and strong
  /// count > 0)
  bool tryPromote() noexcept {
    // Can only promote if resource has been created
    if (!slot_.isCreated()) {
      return false;
    }
    std::uint32_t current = strong_count_.load(std::memory_order_acquire);
    while (current > 0) {
      if (strong_count_.compare_exchange_weak(current, current + 1,
                                              std::memory_order_acquire,
                                              std::memory_order_relaxed)) {
        return true;
      }
    }
    return false;
  }

  // =========================================================================
  // Payload Access
  // =========================================================================

  /// @brief Access the payload
  Payload &payload() noexcept { return slot_.get(); }
  const Payload &payload() const noexcept { return slot_.get(); }

  // =========================================================================
  // Generation (delegated to Slot)
  // =========================================================================

  /// @brief Get current generation (0 if not supported)
  auto generation() const noexcept { return slot_.generation(); }

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
  std::atomic<std::uint32_t> strong_count_{0};
  std::atomic<std::uint32_t> weak_count_{0};
  SlotT slot_{};
};

} // namespace orteaf::internal::runtime::base

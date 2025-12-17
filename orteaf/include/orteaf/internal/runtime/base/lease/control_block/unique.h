#pragma once

#include <atomic>
#include <concepts>
#include <utility>

#include <orteaf/internal/runtime/base/lease/category.h>
#include <orteaf/internal/runtime/base/lease/slot.h>

namespace orteaf::internal::runtime::base {

/// @brief Unique control block - single ownership with in_use flag
/// @details Only one lease can hold this resource at a time.
/// Uses atomic CAS for thread-safe acquisition.
/// isAlive() returns the in_use state.
template <typename SlotT>
  requires SlotConcept<SlotT>
class UniqueControlBlock {
public:
  using Category = lease_category::Unique;
  using Slot = SlotT;
  using Payload = typename SlotT::Payload;

  UniqueControlBlock() = default;
  UniqueControlBlock(const UniqueControlBlock &) = delete;
  UniqueControlBlock &operator=(const UniqueControlBlock &) = delete;

  UniqueControlBlock(UniqueControlBlock &&other) noexcept
      : slot_(std::move(other.slot_)) {
    in_use_.store(other.in_use_.load(std::memory_order_relaxed),
                  std::memory_order_relaxed);
  }

  UniqueControlBlock &operator=(UniqueControlBlock &&other) noexcept {
    if (this != &other) {
      in_use_.store(other.in_use_.load(std::memory_order_relaxed),
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

  /// @brief Release ownership (for reuse)
  /// @return true if was in use and now released, false if wasn't in use
  bool release() noexcept {
    bool expected = true;
    if (in_use_.compare_exchange_strong(expected, false,
                                        std::memory_order_release,
                                        std::memory_order_relaxed)) {
      if constexpr (SlotT::has_generation) {
        slot_.incrementGeneration();
      }
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
      bool destroyed = slot_.destroy(std::forward<DestroyFn>(destroyFn));
      if constexpr (SlotT::has_generation) {
        slot_.incrementGeneration();
      }
      return destroyed;
    }
    return false;
  }

  /// @brief Check if resource is currently acquired
  bool isAlive() const noexcept {
    return in_use_.load(std::memory_order_acquire);
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
  SlotT slot_{};
};

} // namespace orteaf::internal::runtime::base

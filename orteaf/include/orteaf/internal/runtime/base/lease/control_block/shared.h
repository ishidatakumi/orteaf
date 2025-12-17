#pragma once

#include <atomic>
#include <concepts>
#include <cstdint>
#include <utility>

#include <orteaf/internal/runtime/base/lease/category.h>
#include <orteaf/internal/runtime/base/lease/slot.h>

namespace orteaf::internal::runtime::base {

/// @brief Shared control block - shared ownership with reference counting
/// @details Multiple leases can share this resource. Uses atomic reference
/// count for thread-safe sharing.
/// isAlive() returns true when count > 0.
template <typename SlotT>
  requires SlotConcept<SlotT>
class SharedControlBlock {
public:
  using Category = lease_category::Shared;
  using Slot = SlotT;
  using Payload = typename SlotT::Payload;

  SharedControlBlock() = default;
  SharedControlBlock(const SharedControlBlock &) = delete;
  SharedControlBlock &operator=(const SharedControlBlock &) = delete;

  SharedControlBlock(SharedControlBlock &&other) noexcept
      : slot_(std::move(other.slot_)) {
    strong_count_.store(other.strong_count_.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
  }

  SharedControlBlock &operator=(SharedControlBlock &&other) noexcept {
    if (this != &other) {
      strong_count_.store(other.strong_count_.load(std::memory_order_relaxed),
                          std::memory_order_relaxed);
      slot_ = std::move(other.slot_);
    }
    return *this;
  }

  // =========================================================================
  // Lifecycle API
  // =========================================================================

  /// @brief Acquire a shared reference, creating resource if needed
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

  /// @brief Release a shared reference (for reuse)
  /// @return true if this was the last reference (count goes 1->0)
  bool release() noexcept {
    if (strong_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      if constexpr (SlotT::has_generation) {
        slot_.incrementGeneration();
      }
      return true;
    }
    return false;
  }

  /// @brief Release and destroy the resource (non-reusable)
  /// @tparam DestroyFn Callable that takes Payload&
  /// @return true if last reference and destroyed, false otherwise
  template <typename DestroyFn>
    requires std::invocable<DestroyFn, Payload &>
  bool releaseAndDestroy(DestroyFn &&destroyFn) {
    if (strong_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      bool destroyed = slot_.destroy(std::forward<DestroyFn>(destroyFn));
      if constexpr (SlotT::has_generation) {
        slot_.incrementGeneration();
      }
      return destroyed;
    }
    return false;
  }

  /// @brief Check if resource is currently acquired
  bool isAlive() const noexcept { return count() > 0; }

  // =========================================================================
  // Shared-specific API (SharedControlBlockConcept)
  // =========================================================================

  /// @brief Get current reference count
  std::uint32_t count() const noexcept {
    return strong_count_.load(std::memory_order_acquire);
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
  std::atomic<std::uint32_t> strong_count_{0};
  SlotT slot_{};
};

} // namespace orteaf::internal::runtime::base

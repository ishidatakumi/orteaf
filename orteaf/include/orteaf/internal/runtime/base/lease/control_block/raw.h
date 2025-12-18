#pragma once

#include <atomic>
#include <concepts>
#include <utility>

#include <orteaf/internal/runtime/base/lease/category.h>
#include <orteaf/internal/runtime/base/lease/slot.h>

namespace orteaf::internal::runtime::base {

/// @brief Raw control block - no reference counting
/// @details Used for resources that don't need lifecycle management.
/// canTeardown() always returns true since Raw resources have no
/// strong reference semantics (they are "always weak").
template <typename SlotT>
  requires SlotConcept<SlotT>
class RawControlBlock {
  // Raw pattern uses cache semantics - generation doesn't make sense
  static_assert(!SlotT::has_generation,
                "RawControlBlock cannot use slots with generation tracking");

public:
  using Category = lease_category::Raw;
  using Slot = SlotT;
  using Payload = typename SlotT::Payload;

  RawControlBlock() = default;
  RawControlBlock(const RawControlBlock &) = delete;
  RawControlBlock &operator=(const RawControlBlock &) = delete;

  RawControlBlock(RawControlBlock &&other) noexcept
      : weak_count_(other.weak_count_.load(std::memory_order_relaxed)),
        slot_(std::move(other.slot_)) {}

  RawControlBlock &operator=(RawControlBlock &&other) noexcept {
    if (this != &other) {
      weak_count_.store(other.weak_count_.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
      slot_ = std::move(other.slot_);
    }
    return *this;
  }

  // =========================================================================
  // Lifecycle API
  // =========================================================================

  /// @brief Acquire the resource, creating if needed
  /// @tparam CreateFn Callable that takes Payload& and returns bool
  /// @return true if acquired (and created if needed), false if creation failed
  template <typename CreateFn>
    requires std::invocable<CreateFn, Payload &> &&
             std::convertible_to<std::invoke_result_t<CreateFn, Payload &>,
                                 bool>
  bool acquire(CreateFn &&createFn) noexcept {
    bool success = slot_.create(std::forward<CreateFn>(createFn));
    if (success) {
      weak_count_.fetch_add(1, std::memory_order_relaxed);
    }
    return success;
  }

  /// @brief Release and prepare for reuse (cache pattern)
  /// @note Does NOT increment generation - resource stays valid in cache
  /// @return always true for raw resources
  bool release() noexcept {
    weak_count_.fetch_sub(1, std::memory_order_relaxed);
    return true;
  }

  /// @brief Release and destroy the resource
  /// @tparam DestroyFn Callable that takes Payload&
  /// @return true if destroyed, false if not created
  template <typename DestroyFn>
    requires std::invocable<DestroyFn, Payload &>
  bool releaseAndDestroy(DestroyFn &&destroyFn) {
    bool destroyed = slot_.destroy(std::forward<DestroyFn>(destroyFn));
    if constexpr (SlotT::has_generation) {
      slot_.incrementGeneration();
    }
    if (destroyed) {
      weak_count_.fetch_sub(1, std::memory_order_relaxed);
    }
    return destroyed;
  }

  /// @brief Check if teardown is allowed
  /// @note Raw resources are "always weak" - teardown is always allowed
  /// @return Always true for Raw (no strong reference blocking)
  bool canTeardown() const noexcept { return true; }

  /// @brief Check if shutdown is allowed
  /// @note Returns true only when no references exist (for safety)
  bool canShutdown() const noexcept { return weak_count_ == 0; }

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
  std::atomic<std::uint32_t> weak_count_{0};
  SlotT slot_{};
};

} // namespace orteaf::internal::runtime::base

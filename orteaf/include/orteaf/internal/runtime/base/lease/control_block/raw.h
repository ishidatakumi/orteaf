#pragma once

#include <concepts>
#include <utility>

#include <orteaf/internal/runtime/base/lease/category.h>
#include <orteaf/internal/runtime/base/lease/slot.h>

namespace orteaf::internal::runtime::base {

/// @brief Raw control block - no reference counting
/// @details Used for resources that don't need lifecycle management.
/// isAlive() simply returns isCreated() since Raw resources have no
/// acquisition semantics.
template <typename SlotT>
  requires SlotConcept<SlotT>
class RawControlBlock {
public:
  using Category = lease_category::Raw;
  using Slot = SlotT;
  using Payload = typename SlotT::Payload;

  RawControlBlock() = default;
  RawControlBlock(const RawControlBlock &) = default;
  RawControlBlock &operator=(const RawControlBlock &) = default;
  RawControlBlock(RawControlBlock &&) = default;
  RawControlBlock &operator=(RawControlBlock &&) = default;

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
    return slot_.create(std::forward<CreateFn>(createFn));
  }

  /// @brief Release and prepare for reuse (no-op for raw)
  /// @return always true for raw resources
  bool release() noexcept {
    if constexpr (SlotT::has_generation) {
      slot_.incrementGeneration();
    }
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
    return destroyed;
  }

  /// @brief Check if resource is alive (for Raw, this means created)
  bool isAlive() const noexcept { return slot_.isCreated(); }

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
  SlotT slot_{};
};

} // namespace orteaf::internal::runtime::base

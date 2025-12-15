#pragma once

#include <orteaf/internal/runtime/base/lease/category.h>
#include <orteaf/internal/runtime/base/lease/concepts.h>
#include <orteaf/internal/runtime/base/lease/slot.h>

namespace orteaf::internal::runtime::base {

/// @brief Raw control block - no reference counting, only slot
/// @details Used for resources that don't need lifecycle management.
/// All operations are no-op.
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
  // Concept Required API
  // =========================================================================

  /// @brief Always succeeds (no tracking)
  constexpr bool acquire() noexcept { return true; }

  /// @brief Release and prepare for reuse
  /// @return always true for raw resources
  bool release() noexcept {
    if constexpr (SlotT::has_generation) {
      slot_.incrementGeneration();
    }
    return true;
  }

  /// @brief Always alive (no tracking)
  constexpr bool isAlive() const noexcept { return true; }

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

  /// @brief Get current generation (0 if not supported)
  auto generation() const noexcept { return slot_.generation(); }

private:
  SlotT slot_{};
};

// Verify concept satisfaction
static_assert(ControlBlockConcept<RawControlBlock<RawSlot<int>>>);

} // namespace orteaf::internal::runtime::base

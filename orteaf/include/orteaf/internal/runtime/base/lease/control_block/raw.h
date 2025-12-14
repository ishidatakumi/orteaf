#pragma once

#include <orteaf/internal/runtime/base/lease/category.h>
#include <orteaf/internal/runtime/base/lease/concepts.h>

namespace orteaf::internal::runtime::base {

/// @brief Raw control block - no reference counting, only slot
/// @details Used for resources that don't need lifecycle management.
/// All operations are no-op. This control block is optimized away in most
/// cases due to empty base optimization.
template <typename SlotT>
  requires SlotConcept<SlotT>
struct RawControlBlock {
  using Category = lease_category::Raw;
  using Slot = SlotT;

  SlotT slot{};

  /// @brief Always succeeds (no actual acquire needed)
  constexpr bool tryAcquire() noexcept { return true; }

  /// @brief No-op release
  constexpr void release() noexcept {}

  /// @brief Always alive (no tracking)
  constexpr bool isAlive() const noexcept { return true; }

  /// @brief Always released (no tracking)
  constexpr bool isReleased() const noexcept { return true; }

  /// @brief Prepare for reuse - increments generation if available
  /// @return always true (no state to validate)
  bool prepareForReuse() noexcept {
    if constexpr (SlotT::has_generation) {
      slot.incrementGeneration();
    }
    return true;
  }
};

// Verify concept satisfaction
static_assert(ControlBlockConcept<RawControlBlock<int>>);

} // namespace orteaf::internal::runtime::base

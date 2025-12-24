#pragma once

#include <concepts>
#include <cstddef>
#include <type_traits>

namespace orteaf::internal::execution::base {

// =============================================================================
// Base ControlBlock Concept
// =============================================================================

/// @brief Base concept for all control blocks
/// @details Minimal interface - essential lifecycle operations:
/// - acquire: take ownership (template, can't check in concept)
/// - release: release ownership for reuse, returns bool
/// - releaseAndDestroy: release and destroy resource (template)
/// - canTeardown: check if teardown is allowed (no strong refs blocking)
/// - isCreated: check if resource has been created
template <typename CB>
concept ControlBlockConcept = requires(CB cb, const CB ccb) {
  typename CB::Category;
  typename CB::Slot;
  // Note: acquire() is a template so can't be checked here
  { cb.release() } -> std::same_as<bool>;
  { ccb.canTeardown() } -> std::same_as<bool>;
  { ccb.canShutdown() } -> std::same_as<bool>;
  { ccb.isCreated() } -> std::same_as<bool>;
  // Note: releaseAndDestroy() is a template so can't be checked here
};

// =============================================================================
// Specialized ControlBlock Concepts
// =============================================================================

/// @brief Concept for shared control blocks (reference counted)
template <typename CB>
concept SharedControlBlockConcept =
    ControlBlockConcept<CB> && requires(CB cb, const CB ccb) {
      { cb.acquire() };
      { ccb.count() } -> std::convertible_to<std::size_t>;
    };

/// @brief Concept for control blocks supporting weak references
template <typename CB>
concept WeakableControlBlockConcept =
    ControlBlockConcept<CB> && requires(CB cb) {
      { cb.acquireWeak() };
      { cb.releaseWeak() } -> std::same_as<bool>;
    };

/// @brief Concept for weakable control blocks that can promote weak to strong
template <typename CB>
concept PromotableControlBlockConcept =
    WeakableControlBlockConcept<CB> && requires(CB cb) {
      { cb.tryPromote() } -> std::same_as<bool>;
    };

// =============================================================================
// Lease/ControlBlock Compatibility Concept
// =============================================================================

/// @brief Concept to check if a Lease type is compatible with a ControlBlock
/// type
/// @details Checks if CompatibleCategory of Lease matches Category of
/// ControlBlock
template <typename LeaseT, typename ControlBlockT>
concept CompatibleLeaseControlBlock =
    requires {
      typename LeaseT::CompatibleCategory;
      typename ControlBlockT::Category;
    } && std::same_as<typename LeaseT::CompatibleCategory,
                      typename ControlBlockT::Category>;

// =============================================================================
// Payload Concept
// =============================================================================

/// @brief Concept for payload types that can be stored in slots
template <typename P>
concept PayloadConcept =
    std::is_default_constructible_v<P> && std::is_move_constructible_v<P> &&
    std::is_move_assignable_v<P>;

} // namespace orteaf::internal::execution::base

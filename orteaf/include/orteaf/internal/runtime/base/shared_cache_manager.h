#pragma once

#include <cstddef>
#include <string>

#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/runtime/base/base_manager.h"

namespace orteaf::internal::runtime::base {

/**
 * @brief State for SharedCacheManager.
 * @tparam Resource The resource type being managed.
 */
template <typename Resource> struct SharedCacheState {
  Resource resource{};
  std::size_t use_count{0};
  bool alive{false};
};

/**
 * @brief Base manager for immutable resources with shared access.
 *
 * Resources are cached and persist until shutdown. Multiple users can
 * share access (use_count tracked). Key-based lookup in derived classes.
 *
 * @tparam Derived CRTP derived class.
 * @tparam Traits Traits class with OpsType, StateType, Name.
 */
template <typename Derived, typename Traits>
class SharedCacheManager : public BaseManager<Derived, Traits> {
public:
  using Base = BaseManager<Derived, Traits>;
  using Ops = typename Traits::OpsType;
  using State = typename Traits::StateType;

  using Base::ensureInitialized;
  using Base::growth_chunk_size_;
  using Base::initialized_;
  using Base::ops_;
  using Base::states_;

protected:
  // ===== Use Count Helpers =====

  /// Increment use_count for the given slot. Returns new count.
  std::size_t incrementUseCount(std::size_t index) {
    return ++states_[index].use_count;
  }

  /// Decrement use_count for the given slot. Returns new count.
  std::size_t decrementUseCount(std::size_t index) {
    if (states_[index].use_count > 0) {
      return --states_[index].use_count;
    }
    return 0;
  }

  /// Get current use_count for the given slot.
  std::size_t useCount(std::size_t index) const {
    return states_[index].use_count;
  }

  /// Check if use_count is zero (no active users).
  bool isUseCountZero(std::size_t index) const { return useCount(index) == 0; }

  // ===== Slot Management =====

  /// Cache managers don't use free_list - resources persist until shutdown.
  /// Allocates new slot at end of states.
  std::size_t allocateSlot() {
    std::size_t current_size = states_.size();
    states_.resize(current_size + 1);
    return current_size;
  }

  /// Mark slot as alive with initial use_count of 1.
  void markSlotAlive(std::size_t index) {
    State &state = states_[index];
    state.alive = true;
    state.use_count = 1;
  }

  /// Check if slot is alive (resource exists).
  bool isSlotAlive(std::size_t index) const {
    return index < states_.size() && states_[index].alive;
  }

  // ===== Combined Helpers =====

  /// Create a handle from index.
  template <typename HandleType>
  HandleType createHandle(std::size_t index) const {
    return HandleType{static_cast<typename HandleType::index_type>(index)};
  }

  /// Validate handle and get state (throws on invalid).
  template <typename HandleType> State &validateAndGetState(HandleType handle) {
    ensureInitialized();
    const std::size_t index = static_cast<std::size_t>(handle.index);
    if (index >= states_.size()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          std::string(Traits::Name) + " handle out of range");
    }
    State &state = states_[index];
    if (!state.alive) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          std::string(Traits::Name) + " is inactive");
    }
    return state;
  }

  /// Clear all cache states during shutdown.
  void clearCacheStates() { states_.clear(); }
};

} // namespace orteaf::internal::runtime::base

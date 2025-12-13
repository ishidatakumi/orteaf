#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/runtime/base/base_manager.h"

namespace orteaf::internal::runtime::base {

/**
 * @brief State for ExclusivePoolManager.
 * @tparam Resource The resource type being managed.
 * @tparam Generation The generation type for stale handle detection.
 */
template <typename Resource, typename Generation = std::uint32_t>
struct ExclusivePoolState {
  Resource resource{};
  Generation generation{0};
  bool alive{false};
  bool in_use{false};
};

/**
 * @brief Base manager for reusable resources with exclusive access.
 *
 * Resources are acquired exclusively (one user at a time) and returned
 * to the pool for reuse after release.
 *
 * @tparam Derived CRTP derived class.
 * @tparam Traits Traits class with OpsType, StateType, Name.
 */
template <typename Derived, typename Traits>
class ExclusivePoolManager : public BaseManager<Derived, Traits> {
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
  // ===== Slot Management =====

  /// Mark slot as in use.
  void markSlotInUse(std::size_t index) { states_[index].in_use = true; }

  /// Mark slot as not in use.
  void markSlotFree(std::size_t index) { states_[index].in_use = false; }

  /// Check if slot is currently in use.
  bool isSlotInUse(std::size_t index) const {
    return index < states_.size() && states_[index].in_use;
  }

  /// Mark slot as alive (resource created).
  void markSlotAlive(std::size_t index) { states_[index].alive = true; }

  /// Check if slot is alive (resource exists).
  bool isSlotAlive(std::size_t index) const {
    return index < states_.size() && states_[index].alive;
  }

  /// Release slot back to pool. Increments generation.
  void releaseSlot(std::size_t index) {
    if (index < states_.size()) {
      states_[index].in_use = false;
      ++states_[index].generation;
      Base::free_list_.pushBack(index);
    }
  }

  // ===== Generation Helpers =====

  /// Check if handle generation matches.
  template <typename HandleType>
  bool isGenerationValid(std::size_t index, HandleType handle) const {
    return index < states_.size() &&
           static_cast<std::size_t>(states_[index].generation) ==
               static_cast<std::size_t>(handle.generation);
  }

  // ===== Combined Helpers =====

  /// Create a handle from index and current generation.
  template <typename HandleType>
  HandleType createHandle(std::size_t index) const {
    return HandleType{static_cast<typename HandleType::index_type>(index),
                      static_cast<typename HandleType::generation_type>(
                          states_[index].generation)};
  }

  /// Validate handle for operations (throws on invalid).
  template <typename HandleType> State &validateAndGetState(HandleType handle) {
    ensureInitialized();
    const std::size_t index = static_cast<std::size_t>(handle.index);
    if (index >= states_.size()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          std::string(Traits::Name) + " handle out of range");
    }
    State &state = states_[index];
    if (!state.in_use) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          std::string(Traits::Name) + " is inactive");
    }
    if (!isGenerationValid(index, handle)) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          std::string(Traits::Name) + " handle is stale");
    }
    return state;
  }

  /// Get state for release (silent, no throw). Returns nullptr if invalid.
  template <typename HandleType>
  State *getStateForRelease(HandleType handle) noexcept {
    if (!initialized_) {
      return nullptr;
    }
    const std::size_t index = static_cast<std::size_t>(handle.index);
    if (index >= states_.size()) {
      return nullptr;
    }
    State &state = states_[index];
    if (!state.in_use) {
      return nullptr;
    }
    if (!isGenerationValid(index, handle)) {
      return nullptr;
    }
    return &state;
  }

  /// Clear all pool states during shutdown.
  void clearPoolStates() {
    states_.clear();
    Base::free_list_.clear();
  }
};

} // namespace orteaf::internal::runtime::base

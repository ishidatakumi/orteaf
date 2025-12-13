#pragma once

#include <cstddef>

#include "orteaf/internal/runtime/base/base_manager.h"

namespace orteaf::internal::runtime::base {

/**
 * @brief State for ExclusiveCacheManager.
 * @tparam Resource The resource type being managed.
 */
template <typename Resource> struct ExclusiveCacheState {
  Resource resource{};
  bool alive{false};
};

/**
 * @brief Base manager for immutable resources with exclusive access.
 *
 * Resources are cached and persist until shutdown. Exclusive access means
 * only one user at a time. Key-based lookup is implemented in derived classes.
 *
 * @tparam Derived CRTP derived class.
 * @tparam Traits Traits class with OpsType, StateType, Name.
 */
template <typename Derived, typename Traits>
class ExclusiveCacheManager : public BaseManager<Derived, Traits> {
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

  /// Cache managers don't use free_list - resources persist until shutdown.
  /// Allocates new slot at end of states.
  std::size_t allocateSlot() {
    std::size_t current_size = states_.size();
    states_.resize(current_size + 1);
    return current_size;
  }

  /// Mark slot as alive (resource created).
  void markSlotAlive(std::size_t index) { states_[index].alive = true; }

  /// Check if slot is alive (resource exists).
  bool isSlotAlive(std::size_t index) const {
    return index < states_.size() && states_[index].alive;
  }
};

} // namespace orteaf::internal::runtime::base

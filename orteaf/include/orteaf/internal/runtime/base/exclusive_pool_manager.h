#pragma once

#include <cstddef>

#include "orteaf/internal/runtime/base/base_manager.h"

namespace orteaf::internal::runtime::base {

/**
 * @brief State for ExclusivePoolState.
 * @tparam Resource The resource type being managed.
 */
template <typename Resource> struct ExclusivePoolState {
  Resource resource{};
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

  /// Release slot back to pool.
  void releaseSlot(std::size_t index) {
    if (index < states_.size()) {
      states_[index].in_use = false;
      Base::free_list_.pushBack(index);
    }
  }
};

} // namespace orteaf::internal::runtime::base

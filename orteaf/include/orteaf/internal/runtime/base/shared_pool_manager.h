#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "orteaf/internal/runtime/base/base_manager.h"

namespace orteaf::internal::runtime::base {

/**
 * @brief State for SharedPoolManager.
 * @tparam Resource The resource type being managed.
 * @tparam Generation The generation type for stale handle detection.
 */
template <typename Resource, typename Generation = std::uint32_t>
struct SharedPoolState {
  std::atomic<std::size_t> ref_count{0};
  Resource resource{};
  Generation generation{0};
  bool alive{false};
  bool in_use{false};

  SharedPoolState() = default;
  SharedPoolState(const SharedPoolState &) = delete;
  SharedPoolState &operator=(const SharedPoolState &) = delete;
  SharedPoolState(SharedPoolState &&other) noexcept
      : ref_count(other.ref_count.load(std::memory_order_relaxed)),
        resource(other.resource), generation(other.generation),
        alive(other.alive), in_use(other.in_use) {
    other.resource = {};
    other.alive = false;
    other.in_use = false;
  }
  SharedPoolState &operator=(SharedPoolState &&other) noexcept {
    if (this != &other) {
      ref_count.store(other.ref_count.load(std::memory_order_relaxed),
                      std::memory_order_relaxed);
      resource = other.resource;
      generation = other.generation;
      alive = other.alive;
      in_use = other.in_use;
      other.resource = {};
      other.alive = false;
      other.in_use = false;
    }
    return *this;
  }
};

/**
 * @brief Base manager for reusable resources with shared access.
 *
 * Resources can be acquired by multiple users (ref-counted) and returned
 * to the pool when ref_count reaches zero.
 *
 * @tparam Derived CRTP derived class.
 * @tparam Traits Traits class with OpsType, StateType, Name.
 */
template <typename Derived, typename Traits>
class SharedPoolManager : public BaseManager<Derived, Traits> {
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
  // ===== Ref Count Helpers =====

  /// Increment ref_count for the given slot. Returns new count.
  std::size_t incrementRefCount(std::size_t index) {
    return states_[index].ref_count.fetch_add(1, std::memory_order_relaxed) + 1;
  }

  /// Decrement ref_count for the given slot. Returns new count.
  std::size_t decrementRefCount(std::size_t index) {
    return states_[index].ref_count.fetch_sub(1, std::memory_order_acq_rel) - 1;
  }

  /// Get current ref_count for the given slot.
  std::size_t refCount(std::size_t index) const {
    return states_[index].ref_count.load(std::memory_order_relaxed);
  }

  /// Check if ref_count is zero (ready to return to pool).
  bool isRefCountZero(std::size_t index) const { return refCount(index) == 0; }

  // ===== Slot Management =====

  /// Mark slot as in use with initial ref_count of 1.
  void markSlotInUse(std::size_t index) {
    State &state = states_[index];
    state.in_use = true;
    state.ref_count.store(1, std::memory_order_relaxed);
  }

  /// Release slot back to pool. Increments generation.
  void releaseSlot(std::size_t index) {
    if (index < states_.size()) {
      State &state = states_[index];
      state.in_use = false;
      state.ref_count.store(0, std::memory_order_relaxed);
      ++state.generation;
      Base::free_list_.pushBack(index);
    }
  }

  /// Check if slot is currently in use.
  bool isSlotInUse(std::size_t index) const {
    return index < states_.size() && states_[index].in_use;
  }

  /// Check if handle generation matches.
  template <typename HandleType>
  bool isGenerationValid(std::size_t index, HandleType handle) const {
    return index < states_.size() &&
           static_cast<std::size_t>(states_[index].generation) ==
               static_cast<std::size_t>(handle.generation);
  }
};

} // namespace orteaf::internal::runtime::base

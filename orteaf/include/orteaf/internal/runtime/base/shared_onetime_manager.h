#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "orteaf/internal/runtime/base/base_manager.h"

namespace orteaf::internal::runtime::base {

/**
 * @brief State for SharedOneTimeManager.
 * @tparam Resource The resource type being managed.
 */
template <typename Resource> struct SharedOneTimeState {
  std::atomic<std::size_t> ref_count{0};
  Resource resource{};
  std::uint32_t generation{0}; // For stale handle detection
  bool alive{false};

  SharedOneTimeState() = default;
  SharedOneTimeState(const SharedOneTimeState &) = delete;
  SharedOneTimeState &operator=(const SharedOneTimeState &) = delete;
  SharedOneTimeState(SharedOneTimeState &&other) noexcept
      : ref_count(other.ref_count.load(std::memory_order_relaxed)),
        resource(std::move(other.resource)), generation(other.generation),
        alive(other.alive) {
    other.resource = {};
    other.generation = 0;
    other.alive = false;
  }
  SharedOneTimeState &operator=(SharedOneTimeState &&other) noexcept {
    if (this != &other) {
      ref_count.store(other.ref_count.load(std::memory_order_relaxed),
                      std::memory_order_relaxed);
      resource = std::move(other.resource);
      generation = other.generation;
      alive = other.alive;
      other.resource = {};
      other.generation = 0;
      other.alive = false;
    }
    return *this;
  }
};

/**
 * @brief Base manager for non-reusable resources with shared access.
 *
 * Resources are destroyed when ref_count reaches zero. Not reused, not cached.
 *
 * @tparam Derived CRTP derived class.
 * @tparam Traits Traits class with OpsType, StateType, Name.
 */
template <typename Derived, typename Traits>
class SharedOneTimeManager : public BaseManager<Derived, Traits> {
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

  /// Check if ref_count is zero (ready to be destroyed).
  bool isRefCountZero(std::size_t index) const { return refCount(index) == 0; }

  // ===== Slot Management =====

  /// Mark slot as alive with initial ref_count of 1.
  void markSlotAlive(std::size_t index) {
    State &state = states_[index];
    state.alive = true;
    state.ref_count.store(1, std::memory_order_relaxed);
  }

  /// Check if slot is alive (resource exists).
  bool isSlotAlive(std::size_t index) const {
    return index < states_.size() && states_[index].alive;
  }

  /// Release slot and return to free list. Resource should be destroyed first.
  void releaseSlotAndDestroy(std::size_t index) {
    if (index < states_.size()) {
      State &state = states_[index];
      state.resource = {};
      state.alive = false;
      state.ref_count.store(0, std::memory_order_relaxed);
      ++state.generation; // Increment for next use
      Base::free_list_.pushBack(index);
    }
  }

  /// Get generation for the given slot.
  std::uint32_t getGeneration(std::size_t index) const {
    return states_[index].generation;
  }

  /// Check if handle generation matches slot generation.
  bool isGenerationValid(std::size_t index, std::uint32_t handle_gen) const {
    return index < states_.size() && states_[index].generation == handle_gen;
  }
};

} // namespace orteaf::internal::runtime::base

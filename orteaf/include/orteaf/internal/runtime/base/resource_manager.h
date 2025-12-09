#pragma once

#include <atomic>
#include <limits>

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/shared_lease.h"
#include "orteaf/internal/runtime/base/base_manager.h"

namespace orteaf::internal::runtime::base {

template <typename Resource, typename Generation = std::uint32_t>
struct GenerationalPoolState {
  std::atomic<std::size_t> ref_count{0};
  Resource resource{};
  Generation generation{0};
  bool alive{false};
  bool in_use{false};

  GenerationalPoolState() = default;
  GenerationalPoolState(const GenerationalPoolState &) = delete;
  GenerationalPoolState &operator=(const GenerationalPoolState &) = delete;
  GenerationalPoolState(GenerationalPoolState &&other) noexcept
      : ref_count(other.ref_count.load(std::memory_order_relaxed)),
        resource(other.resource), generation(other.generation),
        alive(other.alive), in_use(other.in_use) {
    other.resource = {};
    other.alive = false;
    other.in_use = false;
  }
  GenerationalPoolState &operator=(GenerationalPoolState &&other) noexcept {
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

template <typename Derived, typename Traits>
class ResourceManager : public BaseManager<Derived, Traits> {
public:
  using Base = BaseManager<Derived, Traits>;
  using Resource = typename Traits::ResourceType;
  using ResourceHandle = typename Traits::HandleType;
  using ResourceLease =
      ::orteaf::internal::base::SharedLease<ResourceHandle, Resource,
                                            ResourceManager>;

  // Expose generic types for ease of use
  using State = typename Traits::StateType;
  using Device = typename Traits::DeviceType;
  using Ops = typename Traits::OpsType;

  // Use Base members
  using Base::device_;
  using Base::ensureInitialized;
  using Base::growth_chunk_size_;
  using Base::initialized_;
  using Base::ops_;
  using Base::states_;

  void initialize(Device device, Ops *ops, std::size_t capacity) {
    shutdown();
    if (device == nullptr) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          std::string(Traits::Name) + " requires a valid device");
    }
    if (ops == nullptr) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          std::string(Traits::Name) + " requires valid ops");
    }
    if (capacity > ResourceHandle::invalid_index()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          "Requested " + std::string(Traits::Name) +
              " capacity exceeds supported limit");
    }
    device_ = device;
    ops_ = ops;
    states_.clear();
    Base::free_list_.clear();

    if (capacity > 0) {
      Base::growPool(capacity);
    }
    initialized_ = true;
  }

  void shutdown() {
    if (!initialized_) {
      return;
    }
    for (std::size_t i = 0; i < states_.size(); ++i) {
      State &state = states_[i];
      if (state.alive) {
        // If the trait defines a nil value for resource, check it?
        // Assuming resource is valid if alive=true.
        Traits::destroy(ops_, state.resource);
        state.resource = {};
        state.alive = false;
        state.in_use = false;
        state.ref_count.store(0, std::memory_order_relaxed);
      }
    }
    states_.clear();
    Base::free_list_.clear();
    device_ = nullptr;
    ops_ = nullptr;
    initialized_ = false;
  }

  ResourceLease acquire() {
    ensureInitialized();
    const std::size_t index = Base::allocateSlot();
    State &state = states_[index];

    // Create resource if not alive
    if (!state.alive) {
      state.resource = Traits::create(ops_, device_);
      // Assuming create returns nullptr/invalid on failure or throws.
      // Traits::create should handle errors or return null.
      // If it returns null/invalid, we should check.
      // Since ResourceType can be anything, check generic 'invalid'.
      // For pointers, nullptr check.
      if constexpr (std::is_pointer_v<Resource>) {
        if (state.resource == nullptr) {
          ::orteaf::internal::diagnostics::error::throwError(
              ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
              "Failed to create " + std::string(Traits::Name));
        }
      }
      state.alive = true;
      state.generation = 0;
    }

    state.in_use = true;
    state.ref_count.store(1, std::memory_order_relaxed);

    const auto handle =
        ResourceHandle{static_cast<typename ResourceHandle::index_type>(index),
                       static_cast<typename ResourceHandle::generation_type>(
                           state.generation)};
    return ResourceLease{static_cast<Derived *>(this), handle, state.resource};
  }

  ResourceLease acquire(ResourceHandle handle) {
    ensureInitialized();
    const std::size_t index = static_cast<std::size_t>(handle.index);
    if (index >= states_.size()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          std::string(Traits::Name) + " handle out of range");
    }
    State &state = states_[index];
    if (!state.alive || !state.in_use) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          std::string(Traits::Name) + " handle is inactive");
    }
    // Handle generation mismatch
    if (static_cast<std::size_t>(state.generation) !=
        static_cast<std::size_t>(handle.generation)) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          std::string(Traits::Name) + " handle is stale");
    }

    // Increment ref count
    state.ref_count.fetch_add(1, std::memory_order_relaxed);

    return ResourceLease{static_cast<Derived *>(this), handle, state.resource};
  }

  void release(ResourceLease &lease) noexcept {
    release(lease.handle());
    lease.invalidate();
  }

  void release(ResourceHandle handle) {
    if (!initialized_ || device_ == nullptr || ops_ == nullptr) {
      return;
    }
    const std::size_t index = static_cast<std::size_t>(handle.index);
    if (index >= states_.size()) {
      return;
    }
    State &state = states_[index];
    if (!state.alive || !state.in_use) {
      return;
    }
    if (static_cast<std::size_t>(state.generation) !=
        static_cast<std::size_t>(handle.generation)) {
      return;
    }

    // Decrement ref count
    const std::size_t prev =
        state.ref_count.fetch_sub(1, std::memory_order_acq_rel);
    if (prev == 1) {
      // ref_count is now 0, return to free list
      state.in_use = false;
      ++state.generation;
      Base::free_list_.pushBack(index);
    }
  }

#if ORTEAF_ENABLE_TEST
  struct DebugState {
    bool alive{false};
    std::uint32_t generation{0};
    std::size_t growth_chunk_size{0};
  };

  DebugState debugState(ResourceHandle handle) const {
    DebugState snapshot{};
    snapshot.growth_chunk_size = growth_chunk_size_;
    const std::size_t index = static_cast<std::size_t>(handle.index);
    if (index < states_.size()) {
      const State &state = states_[index];
      snapshot.alive = state.alive;
      snapshot.generation = static_cast<std::uint32_t>(state.generation);
    } else {
      snapshot.generation = std::numeric_limits<std::uint32_t>::max();
    }
    return snapshot;
  }
#endif
};

} // namespace orteaf::internal::runtime::base

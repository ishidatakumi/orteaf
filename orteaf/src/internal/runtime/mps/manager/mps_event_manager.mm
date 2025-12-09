#include "orteaf/internal/runtime/mps/manager/mps_event_manager.h"

#if ORTEAF_ENABLE_MPS

namespace orteaf::internal::runtime::mps::manager {

void MpsEventManager::initialize(DeviceType device, SlowOps *slow_ops,
                                 std::size_t capacity) {
  shutdown();
  if (device == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS event manager requires a valid device");
  }
  if (slow_ops == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS event manager requires valid ops");
  }
  if (capacity > ::orteaf::internal::base::EventHandle::invalid_index()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Requested MPS event capacity exceeds supported limit");
  }
  device_ = device;
  ops_ = slow_ops;
  states_.clear();
  free_list_.clear();

  if (capacity > 0) {
    growPool(capacity);
  }
  initialized_ = true;
}

void MpsEventManager::shutdown() {
  if (!initialized_) {
    return;
  }
  for (std::size_t i = 0; i < states_.size(); ++i) {
    State &state = states_[i];
    if (state.alive && state.event != nullptr) {
      ops_->destroyEvent(state.event);
      state.event = nullptr;
      state.alive = false;
      state.in_use = false;
      state.ref_count.store(0, std::memory_order_relaxed);
    }
  }
  states_.clear();
  free_list_.clear();
  device_ = nullptr;
  ops_ = nullptr;
  initialized_ = false;
}

MpsEventManager::EventLease MpsEventManager::acquire() {
  ensureInitialized();
  const std::size_t index = allocateSlot();
  State &state = states_[index];

  // Create event if not alive
  if (!state.alive) {
    state.event = ops_->createEvent(device_);
    if (state.event == nullptr) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "Failed to create MPS event");
    }
    state.alive = true;
    state.generation = 0;
  }

  state.in_use = true;
  state.ref_count.store(1, std::memory_order_relaxed);

  const auto handle = ::orteaf::internal::base::EventHandle{
      static_cast<std::uint32_t>(index),
      static_cast<::orteaf::internal::base::EventHandle::generation_type>(
          state.generation)};
  return EventLease{this, handle, state.event};
}

MpsEventManager::EventLease MpsEventManager::acquire(EventHandle handle) {
  ensureInitialized();
  const std::size_t index = static_cast<std::size_t>(handle.index);
  if (index >= states_.size()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS event handle out of range");
  }
  State &state = states_[index];
  if (!state.alive || !state.in_use) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS event handle is inactive");
  }
  if (static_cast<::orteaf::internal::base::EventHandle::generation_type>(
          state.generation) != handle.generation) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS event handle is stale");
  }

  // Increment ref count
  state.ref_count.fetch_add(1, std::memory_order_relaxed);

  return EventLease{this, handle, state.event};
}

void MpsEventManager::release(EventHandle &handle) {
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
  if (static_cast<::orteaf::internal::base::EventHandle::generation_type>(
          state.generation) != handle.generation) {
    return;
  }

  // Decrement ref count
  const std::size_t prev =
      state.ref_count.fetch_sub(1, std::memory_order_acq_rel);
  if (prev == 1) {
    // ref_count is now 0, return to free list
    state.in_use = false;
    ++state.generation;
    free_list_.pushBack(index);
  }
}

#if ORTEAF_ENABLE_TEST
MpsEventManager::DebugState MpsEventManager::debugState(
    ::orteaf::internal::base::EventHandle handle) const {
  DebugState snapshot{};
  snapshot.growth_chunk_size = growth_chunk_size_;
  const std::size_t index = static_cast<std::size_t>(handle.index);
  if (index < states_.size()) {
    const State &state = states_[index];
    snapshot.alive = state.alive;
    snapshot.generation = state.generation;
  } else {
    snapshot.generation = std::numeric_limits<std::uint32_t>::max();
  }
  return snapshot;
}
#endif

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

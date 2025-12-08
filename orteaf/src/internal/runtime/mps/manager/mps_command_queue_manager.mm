#include <orteaf/internal/runtime/mps/manager/mps_command_queue_manager.h>

#if ORTEAF_ENABLE_MPS

namespace orteaf::internal::runtime::mps::manager {

void MpsCommandQueueManager::initialize(
  ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t device, SlowOps *slow_ops,
    std::size_t capacity) {
  shutdown();
  if (slow_ops == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS command queue manager requires valid ops");
  }
  device_ = device;
  ops_ = slow_ops;

  if (capacity > ::orteaf::internal::base::CommandQueueHandle::invalid_index()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Requested MPS command queue capacity exceeds supported limit");
  }

  states_.clear();
  free_list_.clear();
  states_.reserve(capacity);
  free_list_.reserve(capacity);

  for (std::size_t index = 0; index < capacity; ++index) {
    State state{};
    state.command_queue = ops_->createCommandQueue(device_);
#if ORTEAF_MPS_DEBUG_ENABLED
    state.event = ops_->createEvent(device_);
#endif
    state.resetHazards();
    state.generation = 0;
    state.in_use = false;
    state.on_free_list = true;
    states_.pushBack(std::move(state));
    free_list_.pushBack(index);
  }

  initialized_ = true;
}

void MpsCommandQueueManager::shutdown() {
  for (std::size_t i = 0; i < states_.size(); ++i) {
    states_[i].destroy(ops_);
  }
  states_.clear();
  free_list_.clear();
  device_ = nullptr;
  ops_ = nullptr;
  initialized_ = false;
}

void MpsCommandQueueManager::growCapacity(std::size_t additional) {
  ensureInitialized();
  if (additional == 0) {
    return;
  }
  growStatePool(additional);
}

MpsCommandQueueManager::CommandQueueLease MpsCommandQueueManager::acquire() {
  const std::size_t index = allocateSlot();
  State &state = states_[index];
  state.in_use = true;
  state.resetHazards();
  const auto handle = ::orteaf::internal::base::CommandQueueHandle{static_cast<std::uint32_t>(index),
                                           static_cast<::orteaf::internal::base::CommandQueueHandle::generation_type>(state.generation)};
  return CommandQueueLease{this, handle, state.command_queue};
}

void MpsCommandQueueManager::release(CommandQueueLease& lease) noexcept {
  if (!initialized_ || ops_ == nullptr || !lease) {
    return;
  }
  const auto handle = lease.handle();
  const std::size_t index = static_cast<std::size_t>(handle.index);
  if (index >= states_.size()) {
    return;
  }
  State &state = states_[index];
  if (!state.in_use || static_cast<::orteaf::internal::base::CommandQueueHandle::generation_type>(state.generation) != handle.generation) {
    return;
  }
  state.in_use = false;
#if ORTEAF_MPS_DEBUG_ENABLED
  if (state.event_refcount != 0 || state.serial_refcount != 0) {
    return;
  }
#endif
  state.resetHazards();
  ++state.generation;
  if (!state.on_free_list) {
    state.on_free_list = true;
    free_list_.pushBack(index);
  }
  lease.invalidate();
}

#if ORTEAF_MPS_DEBUG_ENABLED
MpsCommandQueueManager::EventLease MpsCommandQueueManager::acquireEvent(::orteaf::internal::base::CommandQueueHandle handle) {
  State& state = ensureActiveState(handle);
  ++state.event_refcount;
  return EventLease{this, handle, state.event};
}

void MpsCommandQueueManager::release(EventLease& lease) noexcept {
  if (!initialized_ || ops_ == nullptr || !lease) {
    return;
  }
  const auto handle = lease.handle();
  const std::size_t index = static_cast<std::size_t>(handle.index);
  if (index >= states_.size()) {
    return;
  }
  State& state = states_[index];
  if (static_cast<::orteaf::internal::base::CommandQueueHandle::generation_type>(state.generation) != handle.generation) {
    return;
  }
  if (state.event_refcount == 0) {
    return;
  }
  --state.event_refcount;
  if (state.in_use || state.event_refcount != 0 || state.serial_refcount != 0) {
    return;
  }
  state.resetHazards();
  ++state.generation;
  if (!state.on_free_list) {
    state.on_free_list = true;
    free_list_.pushBack(index);
  }
  lease.invalidate();
}

MpsCommandQueueManager::SerialLease MpsCommandQueueManager::acquireSerial(::orteaf::internal::base::CommandQueueHandle handle) {
  State& state = ensureActiveState(handle);
  ++state.serial_refcount;
  return SerialLease{this, handle, &state.serial};
}

void MpsCommandQueueManager::release(SerialLease& lease) noexcept {
  if (!initialized_ || ops_ == nullptr || !lease) {
    return;
  }
  const auto handle = lease.handle();
  const std::size_t index = static_cast<std::size_t>(handle.index);
  if (index >= states_.size()) {
    return;
  }
  State& state = states_[index];
  if (static_cast<::orteaf::internal::base::CommandQueueHandle::generation_type>(state.generation) != handle.generation) {
    return;
  }
  if (state.serial_refcount == 0) {
    return;
  }
  --state.serial_refcount;
  if (state.in_use || state.event_refcount != 0 || state.serial_refcount != 0) {
    return;
  }
  state.resetHazards();
  ++state.generation;
  if (!state.on_free_list) {
    state.on_free_list = true;
    free_list_.pushBack(index);
  }
  lease.invalidate();
}
#endif

void MpsCommandQueueManager::releaseUnusedQueues() {
  ensureInitialized();
  if (states_.empty() || free_list_.empty()) {
    return;
  }
  // All non-free-list entries must be unused before we can destroy free slots.
  for (std::size_t i = 0; i < states_.size(); ++i) {
    const State& state = states_[i];
    if (!state.on_free_list && state.in_use) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "Cannot release unused queues while queues are in use");
    }
#if ORTEAF_MPS_DEBUG_ENABLED
    if (!state.on_free_list && (state.event_refcount != 0 || state.serial_refcount != 0)) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "Cannot release unused queues while debug leases are in use");
    }
#endif
  }
  // Destroy only free-list entries.
  for (std::size_t idx : free_list_) {
    if (idx < states_.size()) {
      states_[idx].destroy(ops_);
    }
  }
  // Compact away destroyed entries (safe because any non-free entries must be inactive).
  ::orteaf::internal::base::HeapVector<State> kept;
  kept.reserve(states_.size() - free_list_.size());
  for (std::size_t i = 0; i < states_.size(); ++i) {
    if (states_[i].on_free_list) {
      continue;
    }
    kept.pushBack(std::move(states_[i]));
  }
  states_ = std::move(kept);
  free_list_.clear();
}

#if ORTEAF_ENABLE_TEST
MpsCommandQueueManager::DebugState
MpsCommandQueueManager::debugState(::orteaf::internal::base::CommandQueueHandle handle) const {
  DebugState snapshot{};
  snapshot.growth_chunk_size = growth_chunk_size_;
  const std::size_t index = static_cast<std::size_t>(handle.index);
  if (index < states_.size()) {
    const State &state = states_[index];
    snapshot.generation = state.generation;
    snapshot.in_use = state.in_use;
    snapshot.queue_allocated = state.command_queue != nullptr;
#if ORTEAF_MPS_DEBUG_ENABLED
    snapshot.submit_serial = state.serial.submit_serial;
    snapshot.completed_serial = state.serial.completed_serial;
    snapshot.event_refcount = state.event_refcount;
    snapshot.serial_refcount = state.serial_refcount;
#endif
  } else {
    snapshot.generation = std::numeric_limits<std::uint32_t>::max();
  }
  return snapshot;
}
#endif

void MpsCommandQueueManagerState::resetHazards() noexcept {
  // Only used in debug builds; keep no-op otherwise.
#if ORTEAF_MPS_DEBUG_ENABLED
  serial.submit_serial = 0;
  serial.completed_serial = 0;
#endif
}

void MpsCommandQueueManagerState::destroy(SlowOps *slow_ops) noexcept {
  if (command_queue != nullptr) {
    slow_ops->destroyCommandQueue(command_queue);
    command_queue = nullptr;
  }
#if ORTEAF_MPS_DEBUG_ENABLED
  if (event != nullptr) {
    slow_ops->destroyEvent(event);
    event = nullptr;
  }
  serial = SerialState{};
  event_refcount = 0;
  serial_refcount = 0;
#endif
  resetHazards();
  in_use = false;
}

std::size_t MpsCommandQueueManager::allocateSlot() {
  ensureInitialized();
  if (free_list_.empty()) {
    growStatePool(growth_chunk_size_);
    if (free_list_.empty()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "No available MPS command queues");
    }
  }
  const std::size_t index = free_list_.back();
  free_list_.resize(free_list_.size() - 1);
  states_[index].on_free_list = false;
  return index;
}

void MpsCommandQueueManager::growStatePool(std::size_t additional_count) {
  if (additional_count == 0) {
    return;
  }
  if (additional_count > (::orteaf::internal::base::CommandQueueHandle::invalid_index() - states_.size())) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Requested MPS command queue capacity exceeds supported limit");
  }
  const std::size_t start_index = states_.size();
  states_.reserve(states_.size() + additional_count);
  free_list_.reserve(states_.size() + additional_count);

  for (std::size_t i = 0; i < additional_count; ++i) {
    State state{};
    state.command_queue = ops_->createCommandQueue(device_);
#if ORTEAF_MPS_DEBUG_ENABLED
    state.event = ops_->createEvent(device_);
#endif
    state.resetHazards();
    state.generation = 0;
    state.in_use = false;
    state.on_free_list = true;
    states_.pushBack(std::move(state));
    free_list_.pushBack(start_index + i);
  }
}

MpsCommandQueueManager::State &
MpsCommandQueueManager::ensureActiveState(::orteaf::internal::base::CommandQueueHandle handle) {
  ensureInitialized();
  const std::size_t index = static_cast<std::size_t>(handle.index);
  if (index >= states_.size()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS command queue handle out of range");
  }
  State &state = states_[index];
  if (!state.in_use) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS command queue is inactive");
  }
  if (static_cast<::orteaf::internal::base::CommandQueueHandle::generation_type>(state.generation) != handle.generation) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS command queue handle is stale");
  }
  return state;
}
} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

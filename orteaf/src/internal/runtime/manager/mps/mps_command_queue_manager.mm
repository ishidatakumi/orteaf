#include "orteaf/internal/runtime/manager/mps/mps_command_queue_manager.h"

#if ORTEAF_ENABLE_MPS

namespace orteaf::internal::runtime::mps {

void MpsCommandQueueManager::initialize(
    ::orteaf::internal::backend::mps::MPSDevice_t device, BackendOps *ops,
    std::size_t capacity) {
  shutdown();
  if (ops == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS command queue manager requires valid ops");
  }
  device_ = device;
  ops_ = ops;

  if (capacity == 0) {
    initialized_ = true;
    return;
  }

  if (capacity > base::CommandQueueId::invalid_index()) {
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
    state.event = ops_->createEvent(device_);
    state.resetHazards();
    state.generation = 0;
    state.in_use = false;
    states_.pushBack(std::move(state));
    free_list_.pushBack(index);
  }

  initialized_ = true;
}

void MpsCommandQueueManager::shutdown() {
  if (states_.empty()) {
    initialized_ = false;
    return;
  }

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

base::CommandQueueId MpsCommandQueueManager::acquire() {
  const std::size_t index = allocateSlot();
  State &state = states_[index];
  state.in_use = true;
  state.resetHazards();
  return encodeId(index, state.generation);
}

void MpsCommandQueueManager::release(base::CommandQueueId id) {
  State &state = ensureActiveState(id);
  if (state.submit_serial != state.completed_serial) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS command queue has in-flight work");
  }
  state.in_use = false;
  state.resetHazards();
  ++state.generation;
  free_list_.pushBack(indexFromId(id));
}

void MpsCommandQueueManager::releaseUnusedQueues() {
  ensureInitialized();
  if (states_.empty() || free_list_.empty()) {
    return;
  }
  if (free_list_.size() != states_.size()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "Cannot release unused queues while queues are in use");
  }
  for (std::size_t i = 0; i < states_.size(); ++i) {
    states_[i].destroy(ops_);
  }
  states_.clear();
  free_list_.clear();
}

::orteaf::internal::backend::mps::MPSCommandQueue_t
MpsCommandQueueManager::getCommandQueue(base::CommandQueueId id) const {
  const State &state = ensureActiveState(id);
  return state.command_queue;
}

std::uint64_t
MpsCommandQueueManager::submitSerial(base::CommandQueueId id) const {
  const State &state = ensureActiveState(id);
  return state.submit_serial;
}

void MpsCommandQueueManager::setSubmitSerial(base::CommandQueueId id,
                                             std::uint64_t value) {
  State &state = ensureActiveState(id);
  state.submit_serial = value;
}

std::uint64_t
MpsCommandQueueManager::completedSerial(base::CommandQueueId id) const {
  const State &state = ensureActiveState(id);
  return state.completed_serial;
}

void MpsCommandQueueManager::setCompletedSerial(base::CommandQueueId id,
                                                std::uint64_t value) {
  State &state = ensureActiveState(id);
  state.completed_serial = value;
}

#if ORTEAF_ENABLE_TEST
MpsCommandQueueManager::DebugState
MpsCommandQueueManager::debugState(base::CommandQueueId id) const {
  DebugState snapshot{};
  snapshot.growth_chunk_size = growth_chunk_size_;
  const std::size_t index = indexFromIdRaw(id);
  if (index < states_.size()) {
    const State &state = states_[index];
    snapshot.submit_serial = state.submit_serial;
    snapshot.completed_serial = state.completed_serial;
    snapshot.generation = state.generation;
    snapshot.in_use = state.in_use;
    snapshot.queue_allocated = state.command_queue != nullptr;
  } else {
    snapshot.generation = std::numeric_limits<std::uint32_t>::max();
  }
  return snapshot;
}
#endif

void MpsCommandQueueManager::State::resetHazards() noexcept {
  submit_serial = 0;
  completed_serial = 0;
}

void MpsCommandQueueManager::State::destroy(BackendOps *ops) noexcept {
  if (event != nullptr) {
    ops->destroyEvent(event);
    event = nullptr;
  }
  if (command_queue != nullptr) {
    ops->destroyCommandQueue(command_queue);
    command_queue = nullptr;
  }
  resetHazards();
  in_use = false;
}

void MpsCommandQueueManager::ensureInitialized() const {
  if (!initialized_ || device_ == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS command queues not initialized");
  }
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
  return index;
}

void MpsCommandQueueManager::growStatePool(std::size_t additional_count) {
  if (additional_count == 0) {
    return;
  }
  if (additional_count > (base::CommandQueueId::invalid_index() - states_.size())) {
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
    state.event = ops_->createEvent(device_);
    state.resetHazards();
    state.generation = 0;
    state.in_use = false;
    states_.pushBack(std::move(state));
    free_list_.pushBack(start_index + i);
  }
}

MpsCommandQueueManager::State &
MpsCommandQueueManager::ensureActiveState(base::CommandQueueId id) {
  ensureInitialized();
  const std::size_t index = indexFromId(id);
  if (index >= states_.size()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS command queue id out of range");
  }
  State &state = states_[index];
  if (!state.in_use) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS command queue is inactive");
  }
  const std::uint32_t expected_generation = generationFromId(id);
  if (static_cast<base::CommandQueueId::generation_type>(state.generation) != expected_generation) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS command queue handle is stale");
  }
  return state;
}

base::CommandQueueId
MpsCommandQueueManager::encodeId(std::size_t index,
                                 std::uint32_t generation) const {
  return base::CommandQueueId{static_cast<std::uint32_t>(index), static_cast<std::uint8_t>(generation)};
}

std::size_t MpsCommandQueueManager::indexFromId(base::CommandQueueId id) const {
  return indexFromIdRaw(id);
}

std::size_t
MpsCommandQueueManager::indexFromIdRaw(base::CommandQueueId id) const {
  return static_cast<std::size_t>(id.index);
}

std::uint32_t
MpsCommandQueueManager::generationFromId(base::CommandQueueId id) const {
  return static_cast<std::uint32_t>(id.generation);
}

} // namespace orteaf::internal::runtime::mps

#endif // ORTEAF_ENABLE_MPS

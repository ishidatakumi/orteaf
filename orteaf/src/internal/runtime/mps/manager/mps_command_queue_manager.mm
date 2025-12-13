#include <orteaf/internal/runtime/mps/manager/mps_command_queue_manager.h>

#if ORTEAF_ENABLE_MPS

namespace orteaf::internal::runtime::mps::manager {

void MpsCommandQueueManager::initialize(
    ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t device,
    SlowOps *slow_ops, std::size_t capacity) {
  shutdown();
  if (slow_ops == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS command queue manager requires valid ops");
  }
  device_ = device;
  ops_ = slow_ops;

  if (capacity >
      ::orteaf::internal::base::CommandQueueHandle::invalid_index()) {
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
    state.generation = 0;
    state.in_use = false;
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
  const auto handle = ::orteaf::internal::base::CommandQueueHandle{
      static_cast<std::uint32_t>(index),
      static_cast<
          ::orteaf::internal::base::CommandQueueHandle::generation_type>(
          state.generation)};
  return CommandQueueLease{this, handle, state.command_queue};
}

void MpsCommandQueueManager::release(CommandQueueLease &lease) noexcept {
  if (!initialized_ || ops_ == nullptr || !lease) {
    return;
  }
  const auto handle = lease.handle();
  const std::size_t index = static_cast<std::size_t>(handle.index);
  if (index >= states_.size()) {
    return;
  }
  State &state = states_[index];
  if (!state.in_use ||
      static_cast<
          ::orteaf::internal::base::CommandQueueHandle::generation_type>(
          state.generation) != handle.generation) {
    return;
  }
  state.in_use = false;
  ++state.generation;
  free_list_.pushBack(index);
  lease.invalidate();
}

void MpsCommandQueueManager::releaseUnusedQueues() {
  ensureInitialized();
  if (states_.empty() || free_list_.empty()) {
    return;
  }
  // All entries must be unused before we can destroy free slots.
  for (std::size_t i = 0; i < states_.size(); ++i) {
    const State &state = states_[i];
    if (state.in_use) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "Cannot release unused queues while queues are in use");
    }
  }
  // Destroy all entries since all are unused.
  for (std::size_t idx = 0; idx < states_.size(); ++idx) {
    states_[idx].destroy(ops_);
  }
  states_.clear();
  free_list_.clear();
}

void MpsCommandQueueManagerState::destroy(SlowOps *slow_ops) noexcept {
  if (command_queue != nullptr) {
    slow_ops->destroyCommandQueue(command_queue);
    command_queue = nullptr;
  }
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
  return index;
}

void MpsCommandQueueManager::growStatePool(std::size_t additional_count) {
  if (additional_count == 0) {
    return;
  }
  if (additional_count >
      (::orteaf::internal::base::CommandQueueHandle::invalid_index() -
       states_.size())) {
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
    state.generation = 0;
    state.in_use = false;
    states_.pushBack(std::move(state));
    free_list_.pushBack(start_index + i);
  }
}

bool MpsCommandQueueManager::isInUse(
    ::orteaf::internal::base::CommandQueueHandle handle) const {
  if (!initialized_) {
    return false;
  }
  const std::size_t index = static_cast<std::size_t>(handle.index);
  if (index >= states_.size()) {
    return false;
  }
  const State &state = states_[index];
  if (static_cast<
          ::orteaf::internal::base::CommandQueueHandle::generation_type>(
          state.generation) != handle.generation) {
    return false;
  }
  return state.in_use;
}

MpsCommandQueueManager::State &MpsCommandQueueManager::ensureActiveState(
    ::orteaf::internal::base::CommandQueueHandle handle) {
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
  if (static_cast<
          ::orteaf::internal::base::CommandQueueHandle::generation_type>(
          state.generation) != handle.generation) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS command queue handle is stale");
  }
  return state;
}
} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

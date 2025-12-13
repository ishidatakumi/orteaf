#include <orteaf/internal/runtime/mps/manager/mps_command_queue_manager.h>

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::runtime::mps::manager {

void MpsCommandQueueManager::initialize(DeviceType device, SlowOps *ops,
                                        std::size_t capacity) {
  shutdown();
  if (ops == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS command queue manager requires valid ops");
  }
  if (capacity >
      static_cast<std::size_t>(CommandQueueHandle::invalid_index())) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS command queue manager capacity exceeds maximum handle range");
  }
  device_ = device;
  ops_ = ops;
  clearPoolStates();

  // Pre-create command queues
  for (std::size_t i = 0; i < capacity; ++i) {
    State state{};
    state.resource = ops_->createCommandQueue(device_);
    state.alive = true;
    state.in_use = false;
    state.generation = 0;
    states_.pushBack(std::move(state));
    Base::free_list_.pushBack(i);
  }
  initialized_ = true;
}

void MpsCommandQueueManager::shutdown() {
  for (std::size_t i = 0; i < states_.size(); ++i) {
    State &state = states_[i];
    if (state.resource != nullptr) {
      ops_->destroyCommandQueue(state.resource);
      state.resource = nullptr;
    }
  }
  clearPoolStates();
  device_ = nullptr;
  ops_ = nullptr;
  initialized_ = false;
}

void MpsCommandQueueManager::growCapacity(std::size_t additional) {
  ensureInitialized();
  if (additional == 0) {
    return;
  }
  const std::size_t start_index = states_.size();
  if (additional > (CommandQueueHandle::invalid_index() - start_index)) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS command queue manager capacity exceeds maximum handle range");
  }
  for (std::size_t i = 0; i < additional; ++i) {
    State state{};
    state.resource = ops_->createCommandQueue(device_);
    state.alive = true;
    state.in_use = false;
    state.generation = 0;
    states_.pushBack(std::move(state));
    Base::free_list_.pushBack(start_index + i);
  }
}

MpsCommandQueueManager::CommandQueueLease MpsCommandQueueManager::acquire() {
  ensureInitialized();
  if (Base::free_list_.empty()) {
    growCapacity(growth_chunk_size_);
  }
  const std::size_t index = Base::allocateSlot();
  State &state = states_[index];
  markSlotInUse(index);
  return CommandQueueLease{this, createHandle<CommandQueueHandle>(index),
                           state.resource};
}

void MpsCommandQueueManager::release(CommandQueueLease &lease) noexcept {
  if (!lease) {
    return;
  }
  State *state = getStateForRelease(lease.handle());
  if (state == nullptr) {
    return;
  }
  releaseSlot(static_cast<std::size_t>(lease.handle().index));
  lease.invalidate();
}

bool MpsCommandQueueManager::isInUse(CommandQueueHandle handle) const {
  if (!initialized_) {
    return false;
  }
  const std::size_t index = static_cast<std::size_t>(handle.index);
  if (index >= states_.size()) {
    return false;
  }
  const State &state = states_[index];
  if (!isGenerationValid(index, handle)) {
    return false;
  }
  return state.in_use;
}

void MpsCommandQueueManager::releaseUnusedQueues() {
  ensureInitialized();
  if (states_.empty() || Base::free_list_.empty()) {
    return;
  }
  for (std::size_t i = 0; i < states_.size(); ++i) {
    if (states_[i].in_use) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "Cannot release unused queues while queues are in use");
    }
  }
  for (std::size_t i = 0; i < states_.size(); ++i) {
    if (states_[i].resource != nullptr) {
      ops_->destroyCommandQueue(states_[i].resource);
      states_[i].resource = nullptr;
    }
  }
  clearPoolStates();
}

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

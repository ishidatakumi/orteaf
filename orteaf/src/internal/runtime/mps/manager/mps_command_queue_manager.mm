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

  // Initialize and pre-create resources (lazy creation is also supported, but
  // this matches previous implementation of pre-creation if capacity > 0)
  // Actually, let's use setupPool with factory to pre-create.
  Base::setupPool(capacity, [&](CommandQueueControlBlock &cb, std::size_t) {
    auto queue = ops_->createCommandQueue(device_);
    if (queue) {
      cb.payload() = queue;
      cb.validate();
    }
    // in_use defaults to false
  });
}

void MpsCommandQueueManager::shutdown() {
  if (!Base::isInitialized()) {
    return;
  }
  // Cleanup all resources
  Base::teardownPool([this](CommandQueueControlBlock &cb, CommandQueueHandle) {
    if (cb.isInitialized()) {
      destroyResource(cb.payload());
      cb.invalidate();
    }
  });
  device_ = nullptr;
  ops_ = nullptr;
}

void MpsCommandQueueManager::growCapacity(std::size_t additional) {
  ensureInitialized();
  if (additional == 0) {
    return;
  }
  const std::size_t current_capacity = Base::capacity();
  const std::size_t max_index =
      static_cast<std::size_t>(CommandQueueHandle::invalid_index());
  if (current_capacity > max_index ||
      additional > (max_index - current_capacity)) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS command queue manager capacity exceeds maximum handle range");
  }

  // Provide a factory to pre-create resources for the new slots
  // expandPool does not take a factory, it only resizes.
  // We need to iterate and initialize manually if we want eager creation.
  const std::size_t start_index =
      Base::expandPool(additional, /*addToFreelist=*/true);

  for (std::size_t i = 0; i < additional; ++i) {
    auto &cb = Base::getControlBlockChecked(
        CommandQueueHandle{static_cast<uint32_t>(start_index + i), 0});
    auto queue = ops_->createCommandQueue(device_);
    if (queue) {
      cb.payload() = queue;
      cb.validate();
    }
  }
}

MpsCommandQueueManager::CommandQueueLease MpsCommandQueueManager::acquire() {
  auto handle = Base::acquireUniqueOrCreate(
      growth_chunk_size_,
      [this](CommandQueueControlBlock &cb, CommandQueueHandle) {
        if (!ops_)
          return false;
        auto queue = ops_->createCommandQueue(device_);
        if (!queue)
          return false;
        cb.payload() = queue;
        return true;
      });

  if (!handle.isValid()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "Failed to create MPS command queue");
  }

  return CommandQueueLease{this, handle,
                           Base::getControlBlockChecked(handle).payload()};
}

void MpsCommandQueueManager::release(CommandQueueLease &lease) noexcept {
  release(lease.handle());
  lease.invalidate();
}

void MpsCommandQueueManager::release(CommandQueueHandle handle) noexcept {
  if (!Base::isValidHandle(handle)) {
    return;
  }
  Base::releaseUnique(handle);
}

bool MpsCommandQueueManager::isInUse(CommandQueueHandle handle) const {
  if (!Base::isInitialized() || !Base::isValidHandle(handle)) {
    return false;
  }
  return Base::getControlBlock(handle).isAlive();
}

void MpsCommandQueueManager::releaseUnusedQueues() {
  ensureInitialized();
  if (Base::inUse() > 0) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "Cannot release unused queues while queues are in use");
  }

  // Destroy all resources and recreate pool (empty) implies teardown
  Base::teardownPool([this](CommandQueueControlBlock &cb, CommandQueueHandle) {
    if (cb.isInitialized()) {
      destroyResource(cb.payload());
      cb.invalidate();
    }
  });

  // Restore state to "Initialized but empty"
  // Previous implementation cleared internal states but kept initialized=true?
  // Let's check original: "clearPoolStates();" does "states_.clear();
  // free_list_.clear();". And "initialized_ = true;" was NOT reset in
  // releaseUnusedQueues? Actually original said: "clearPoolStates();" at end.
  // And "ensureInitialized()" at start.
  // So manager remains initialized but empty.
  Base::setupPoolEmpty();
}

void MpsCommandQueueManager::destroyResource(CommandQueueType &resource) {
  if (resource != nullptr) {
    if (ops_) {
      ops_->destroyCommandQueue(resource);
    }
    resource = nullptr;
  }
}

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

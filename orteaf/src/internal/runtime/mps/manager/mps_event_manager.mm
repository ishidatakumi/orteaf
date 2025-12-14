#include "orteaf/internal/runtime/mps/manager/mps_event_manager.h"

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::runtime::mps::manager {

void MpsEventManager::initialize(DeviceType device, SlowOps *ops,
                                 std::size_t capacity) {
  shutdown();
  if (device == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS event manager requires a valid device");
  }
  if (ops == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS event manager requires valid ops");
  }
  if (capacity > static_cast<std::size_t>(EventHandle::invalid_index())) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS event manager capacity exceeds maximum handle range");
  }
  device_ = device;
  ops_ = ops;

  // Fill pool with capacity
  Base::setupPool(capacity);
}

void MpsEventManager::destroyResource(EventType &resource) {
  if (resource != nullptr) {
    ops_->destroyEvent(resource);
    resource = nullptr;
  }
}

void MpsEventManager::shutdown() {
  if (!Base::isInitialized()) {
    return;
  }
  // Teardown and destroy all initialized resources
  Base::teardownPool([this](EventControlBlock &cb, EventHandle h) {
    if (cb.slot.isInitialized()) {
      destroyResource(cb.slot.get());
    }
  });

  device_ = nullptr;
  ops_ = nullptr;
}

MpsEventManager::EventLease MpsEventManager::acquire() {
  ensureInitialized();

  // acquireOrCreate handles finding a free slot (allocating if needed)
  // and creating the resource if it's not initialized.
  auto handle = Base::acquireOrCreate(
      growth_chunk_size_, [this](EventControlBlock &cb, EventHandle) {
        auto event = ops_->createEvent(device_);
        if (event == nullptr) {
          return false;
        }
        cb.slot.get() = event;
        return true;
      });

  if (!handle.isValid()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "Failed to create MPS event");
  }

  // Handle acquired -> ref count 0->1 managed by acquireOrCreate (via
  // tryAcquire)
  auto &cb = Base::getControlBlock(handle);

  return EventLease{this, handle, cb.slot.get()};
}

MpsEventManager::EventLease MpsEventManager::acquire(EventHandle handle) {
  auto &cb = Base::acquireShared(handle);
  return EventLease{this, handle, cb.slot.get()};
}

void MpsEventManager::release(EventLease &lease) noexcept {
  release(lease.handle());
  lease.invalidate();
}

void MpsEventManager::release(EventHandle handle) noexcept {
  if (!Base::isValidHandle(handle)) {
    return;
  }
  Base::releaseShared(handle);
}

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

#include "orteaf/internal/runtime/mps/manager/mps_fence_manager.h"

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::runtime::mps::manager {

void MpsFenceManager::initialize(DeviceType device, SlowOps *ops,
                                 std::size_t capacity) {
  shutdown();
  if (device == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS fence manager requires a valid device");
  }
  if (ops == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS fence manager requires valid ops");
  }
  if (capacity > static_cast<std::size_t>(FenceHandle::invalid_index())) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS fence manager capacity exceeds maximum handle range");
  }
  device_ = device;
  ops_ = ops;
  Base::setupPool(capacity);
}

void MpsFenceManager::shutdown() {
  if (!Base::isInitialized()) {
    return;
  }
  Base::teardownPool([this](FenceControlBlock &cb, FenceHandle) {
    if (cb.slot.isInitialized()) {
      destroyResource(cb.slot.get());
    }
  });
  device_ = nullptr;
  ops_ = nullptr;
}

MpsFenceManager::FenceLease MpsFenceManager::acquire() {
  auto handle = Base::acquireOrCreate(
      growth_chunk_size_, [this](FenceControlBlock &cb, FenceHandle) {
        if (!ops_) {
          return false;
        }
        auto fence = ops_->createFence(device_);
        if (fence == nullptr) {
          return false;
        }
        cb.slot.get() = fence;
        return true;
      });

  if (!handle.isValid()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "Failed to create MPS fence");
  }

  // Base::acquireOrCreate already calls cb.tryAcquire() for fresh resources
  // If we reused a resource (not fresh), it does not increment if it was
  // already alive. Wait, acquireOrCreate logic:
  // - allocate slot
  // - if not initialized: createFn -> initialized.
  // - tryAcquire() -> increments strong_count from 0 to 1.
  // So handle is owned with count=1.

  return FenceLease{this, handle,
                    Base::getControlBlockChecked(handle).slot.get()};
}

MpsFenceManager::FenceLease MpsFenceManager::acquire(FenceHandle handle) {
  auto &cb = Base::getControlBlockChecked(handle);
  if (cb.count() == 0) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "Cannot acquire shared handle to a released resource");
  }
  cb.acquire();
  return FenceLease{this, handle, cb.slot.get()};
}

void MpsFenceManager::release(FenceLease &lease) noexcept {
  release(lease.handle());
  lease.invalidate();
}

void MpsFenceManager::release(FenceHandle handle) noexcept {
  if (!Base::isValidHandle(handle)) {
    return;
  }
  Base::releaseShared(handle);
}

void MpsFenceManager::destroyResource(FenceType &resource) {
  if (resource != nullptr) {
    if (ops_) {
      ops_->destroyFence(resource);
    }
    resource = nullptr;
  }
}

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

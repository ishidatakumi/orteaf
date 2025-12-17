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
    // Check if resource was created (payload is valid)
    if (cb.payload() != nullptr) {
      destroyResource(cb.payload());
    }
  });
  device_ = nullptr;
  ops_ = nullptr;
}

MpsFenceManager::FenceLease MpsFenceManager::acquire() {
  auto handle = Base::acquireFresh([this](FenceType &payload) {
    if (!ops_) {
      return false;
    }
    auto fence = ops_->createFence(device_);
    if (fence == nullptr) {
      return false;
    }
    payload = fence;
    return true;
  });

  if (!handle.isValid()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "Failed to create MPS fence");
  }

  return FenceLease{this, handle,
                    Base::getControlBlockChecked(handle).payload()};
}

MpsFenceManager::FenceLease MpsFenceManager::acquire(FenceHandle handle) {
  auto &cb = Base::acquireExisting(handle);
  return FenceLease{this, handle, cb.payload()};
}

void MpsFenceManager::release(FenceLease &lease) noexcept {
  release(lease.handle());
  lease.invalidate();
}

void MpsFenceManager::release(FenceHandle handle) noexcept {
  if (!Base::isValidHandle(handle)) {
    return;
  }
  Base::releaseForReuse(handle);
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

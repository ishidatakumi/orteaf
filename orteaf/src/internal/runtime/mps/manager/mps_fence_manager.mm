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

  core_.payloadPool().initialize(FencePayloadPoolTraits::Config{capacity});
  core_.initializeControlBlockPool(capacity);
  core_.setInitialized(true);
}

void MpsFenceManager::shutdown() {
  if (!core_.isInitialized()) {
    return;
  }
  // Check canShutdown on all created control blocks
  core_.checkCanShutdownOrThrow();

  const FencePayloadPoolTraits::Request payload_request{};
  const auto payload_context = makePayloadContext();
  core_.payloadPool().shutdown(payload_request, payload_context);
  core_.shutdownControlBlockPool();

  device_ = nullptr;
  ops_ = nullptr;
  core_.setInitialized(false);
}

MpsFenceManager::FenceLease MpsFenceManager::acquire() {
  core_.ensureInitialized();
  const FencePayloadPoolTraits::Request request{};
  const auto context = makePayloadContext();
  auto payload_ref = core_.payloadPool().tryAcquire(request, context);
  if (!payload_ref.valid()) {
    auto reserved = core_.payloadPool().tryReserve(request, context);
    if (!reserved.valid()) {
      const auto desired =
          core_.payloadPool().capacity() + core_.growthChunkSize();
      growPools(desired);
      reserved = core_.payloadPool().tryReserve(request, context);
    }
    if (!reserved.valid()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::OutOfRange,
          "MPS fence manager has no available slots");
    }
    FencePayloadPoolTraits::Request create_request{reserved.handle};
    if (!core_.payloadPool().emplace(reserved.handle, create_request,
                                     context)) {
      core_.payloadPool().release(reserved.handle);
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "Failed to create MPS fence");
    }
    payload_ref = reserved;
  }

  auto cb_ref = core_.acquireControlBlock();
  auto cb_handle = cb_ref.handle;
  auto *cb = cb_ref.payload_ptr;
  if (cb == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS fence control block is unavailable");
  }
  return buildLease(*cb, payload_ref.handle, cb_handle);
}

FencePayloadPoolTraits::Context
MpsFenceManager::makePayloadContext() const noexcept {
  return FencePayloadPoolTraits::Context{device_, ops_};
}

void MpsFenceManager::growPools(std::size_t desired_capacity) {
  if (desired_capacity <= core_.payloadPool().capacity()) {
    return;
  }
  core_.payloadPool().grow(FencePayloadPoolTraits::Config{desired_capacity});
  // Note: CB pool grows on demand in acquireControlBlock, no need to grow here
}

MpsFenceManager::FenceLease
MpsFenceManager::buildLease(ControlBlock &cb, FenceHandle payload_handle,
                            ControlBlockHandle cb_handle) {
  auto *payload_ptr = core_.payloadPool().get(payload_handle);
  if (payload_ptr == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS fence payload is unavailable");
  }
  if (cb.hasPayload()) {
    if (cb.payloadHandle() != payload_handle) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "MPS fence control block payload mismatch");
    }
  } else if (!cb.tryBindPayload(payload_handle, payload_ptr,
                                &core_.payloadPool())) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS fence control block binding failed");
  }
  return FenceLease{&cb, &core_.controlBlockPool(), cb_handle};
}

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

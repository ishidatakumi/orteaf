#include "orteaf/internal/execution/mps/manager/mps_fence_manager.h"

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::execution::mps::manager {

void MpsFenceManager::configure(const Config &config) {
  shutdown();
  if (config.device == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS fence manager requires a valid device");
  }
  if (config.ops == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS fence manager requires valid ops");
  }
  device_ = config.device;
  ops_ = config.ops;

  const FencePayloadPoolTraits::Request payload_request{};
  const auto payload_context = makePayloadContext();
  core_.configure(config.pool, payload_request, payload_context);
  if (!core_.createAllPayloads(payload_request, payload_context)) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "Failed to create MPS fences");
  }
}

void MpsFenceManager::shutdown() {
  if (!core_.isConfigured()) {
    return;
  }
  // Check canShutdown on all created control blocks

  const FencePayloadPoolTraits::Request payload_request{};
  const auto payload_context = makePayloadContext();
  core_.shutdown(payload_request, payload_context);

  device_ = nullptr;
  ops_ = nullptr;
}

MpsFenceManager::FenceLease MpsFenceManager::acquire() {
  core_.ensureConfigured();
  const FencePayloadPoolTraits::Request request{};
  const auto context = makePayloadContext();
  auto handle = core_.acquirePayloadOrGrowAndCreate(request, context);
  if (!handle.isValid()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::OutOfRange,
        "MPS fence manager has no available slots");
  }
  return core_.acquireStrongLease(handle);
}

MpsFenceManager::FenceWeakLease
MpsFenceManager::acquireWeak(FenceHandle handle) {
  core_.ensureConfigured();
  return core_.acquireWeakLease(handle);
}

FencePayloadPoolTraits::Context
MpsFenceManager::makePayloadContext() const noexcept {
  return FencePayloadPoolTraits::Context{device_, ops_};
}

} // namespace orteaf::internal::execution::mps::manager

#endif // ORTEAF_ENABLE_MPS

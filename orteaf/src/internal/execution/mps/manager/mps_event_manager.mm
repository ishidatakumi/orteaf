#include "orteaf/internal/execution/mps/manager/mps_event_manager.h"

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::execution::mps::manager {

void MpsEventManager::configure(const Config &config) {
  shutdown();
  if (config.device == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS event manager requires a valid device");
  }
  if (config.ops == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS event manager requires valid ops");
  }
  const std::size_t payload_capacity =
      config.payload_capacity != 0 ? config.payload_capacity : 0u;
  const std::size_t control_block_capacity =
      config.control_block_capacity != 0 ? config.control_block_capacity
                                         : payload_capacity;
  if (payload_capacity >
      static_cast<std::size_t>(EventHandle::invalid_index())) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS event manager capacity exceeds maximum handle range");
  }
  if (control_block_capacity >
      static_cast<std::size_t>(Core::ControlBlockHandle::invalid_index())) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS event manager control block capacity exceeds maximum handle range");
  }
  if (payload_capacity != 0 && config.payload_block_size == 0) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS event manager requires non-zero payload block size");
  }
  if (config.control_block_block_size == 0) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS event manager requires non-zero control block size");
  }
  if (config.payload_growth_chunk_size == 0) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS event manager requires non-zero payload growth chunk size");
  }
  if (config.control_block_growth_chunk_size == 0) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS event manager requires non-zero control block growth chunk size");
  }
  device_ = config.device;
  ops_ = config.ops;
  payload_block_size_ = config.payload_block_size;
  payload_growth_chunk_size_ = config.payload_growth_chunk_size;

  core_.payloadPool().configure(
      EventPayloadPool::Config{payload_capacity, config.payload_block_size});
  const EventPayloadPoolTraits::Request payload_request{};
  const auto payload_context = makePayloadContext();
  if (!core_.payloadPool().createAll(payload_request, payload_context)) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "Failed to create MPS events");
  }
  core_.configure(MpsEventManager::Core::Config{
      /*control_block_capacity=*/control_block_capacity,
      /*control_block_block_size=*/config.control_block_block_size,
      /*growth_chunk_size=*/config.control_block_growth_chunk_size});
  core_.setInitialized(true);
}

void MpsEventManager::shutdown() {
  if (!core_.isInitialized()) {
    return;
  }
  // Check canShutdown on all created control blocks
  core_.checkCanShutdownOrThrow();

  const EventPayloadPoolTraits::Request payload_request{};
  const auto payload_context = makePayloadContext();
  core_.payloadPool().shutdown(payload_request, payload_context);
  core_.shutdownControlBlockPool();

  device_ = nullptr;
  ops_ = nullptr;
  core_.setInitialized(false);
}

MpsEventManager::EventLease MpsEventManager::acquire() {
  core_.ensureInitialized();
  const EventPayloadPoolTraits::Request request{};
  const auto context = makePayloadContext();
  auto payload_ref = core_.acquirePayloadOrGrowAndCreate(
      payload_growth_chunk_size_, request, context);
  if (!payload_ref.valid()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::OutOfRange,
        "MPS event manager has no available slots");
  }

  auto cb_ref = core_.acquireControlBlock();
  auto cb_handle = cb_ref.handle;
  auto *cb = cb_ref.payload_ptr;
  if (cb == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS event control block is unavailable");
  }
  return buildLease(*cb, payload_ref.handle, cb_handle);
}

EventPayloadPoolTraits::Context
MpsEventManager::makePayloadContext() const noexcept {
  return EventPayloadPoolTraits::Context{device_, ops_};
}

MpsEventManager::EventLease
MpsEventManager::buildLease(ControlBlock &cb, EventHandle payload_handle,
                            ControlBlockHandle cb_handle) {
  auto *payload_ptr = core_.payloadPool().get(payload_handle);
  if (payload_ptr == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS event payload is unavailable");
  }
  if (cb.hasPayload()) {
    if (cb.payloadHandle() != payload_handle) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "MPS event control block payload mismatch");
    }
  } else if (!cb.tryBindPayload(payload_handle, payload_ptr,
                                &core_.payloadPool())) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS event control block binding failed");
  }
  return EventLease{&cb, core_.controlBlockPoolForLease(), cb_handle};
}

} // namespace orteaf::internal::execution::mps::manager

#endif // ORTEAF_ENABLE_MPS

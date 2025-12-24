#include "orteaf/internal/execution/mps/manager/mps_command_queue_manager.h"

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::execution::mps::manager {

void MpsCommandQueueManager::configure(const Config &config) {
  shutdown();
  if (config.device == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS command queue manager requires a valid device");
  }
  if (config.ops == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS command queue manager requires valid ops");
  }
  const std::size_t payload_capacity =
      config.payload_capacity != 0 ? config.payload_capacity : 0u;
  const std::size_t control_block_capacity =
      config.control_block_capacity != 0 ? config.control_block_capacity
                                         : payload_capacity;
  if (payload_capacity >
      static_cast<std::size_t>(CommandQueueHandle::invalid_index())) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS command queue manager capacity exceeds maximum handle range");
  }
  if (control_block_capacity >
      static_cast<std::size_t>(Core::ControlBlockHandle::invalid_index())) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS command queue manager control block capacity exceeds maximum handle range");
  }
  if (payload_capacity != 0 && config.payload_block_size == 0) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS command queue manager requires non-zero payload block size");
  }
  if (config.control_block_block_size == 0) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS command queue manager requires non-zero control block size");
  }
  if (config.payload_growth_chunk_size == 0) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS command queue manager requires non-zero payload growth chunk size");
  }
  if (config.control_block_growth_chunk_size == 0) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS command queue manager requires non-zero control block growth chunk size");
  }
  device_ = config.device;
  ops_ = config.ops;
  payload_block_size_ = config.payload_block_size;
  payload_growth_chunk_size_ = config.payload_growth_chunk_size;

  core_.payloadPool().configure(CommandQueuePayloadPool::Config{
      payload_capacity, config.payload_block_size});
  const CommandQueuePayloadPoolTraits::Request payload_request{};
  const CommandQueuePayloadPoolTraits::Context payload_context{device_, ops_};
  if (!core_.payloadPool().createAll(payload_request, payload_context)) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "Failed to create MPS command queues");
  }
  core_.configure(MpsCommandQueueManager::Core::Config{
      /*control_block_capacity=*/control_block_capacity,
      /*control_block_block_size=*/config.control_block_block_size,
      /*growth_chunk_size=*/config.control_block_growth_chunk_size});
  core_.setInitialized(true);
}

void MpsCommandQueueManager::shutdown() {
  if (!core_.isInitialized()) {
    return;
  }
  // Check canShutdown on all created control blocks
  core_.checkCanShutdownOrThrow();

  const CommandQueuePayloadPoolTraits::Request payload_request{};
  const CommandQueuePayloadPoolTraits::Context payload_context{device_, ops_};
  core_.payloadPool().shutdown(payload_request, payload_context);
  core_.shutdownControlBlockPool();

  device_ = nullptr;
  ops_ = nullptr;
  core_.setInitialized(false);
}

MpsCommandQueueManager::CommandQueueLease MpsCommandQueueManager::acquire() {
  core_.ensureInitialized();
  const CommandQueuePayloadPoolTraits::Request request{};
  const CommandQueuePayloadPoolTraits::Context context{device_, ops_};

  auto payload_ref = core_.acquirePayloadOrGrowAndCreate(
      payload_growth_chunk_size_, request, context);
  if (!payload_ref.valid()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::OutOfRange,
        "MPS command queue manager has no available slots");
  }

  auto cb_ref = core_.acquireControlBlock();
  auto cb_handle = cb_ref.handle;
  auto *cb = cb_ref.payload_ptr;
  if (cb == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS command queue control block is unavailable");
  }

  auto *payload_ptr = core_.payloadPool().get(payload_ref.handle);
  if (payload_ptr == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS command queue payload is unavailable");
  }
  if (!cb->tryBindPayload(payload_ref.handle, payload_ptr,
                          &core_.payloadPool())) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS command queue control block binding failed");
  }
  return CommandQueueLease{cb, core_.controlBlockPoolForLease(), cb_handle};
}

MpsCommandQueueManager::CommandQueueLease
MpsCommandQueueManager::acquire(CommandQueueHandle handle) {
  core_.ensureInitialized();

  if (!handle.isValid()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Invalid command queue handle");
  }

  auto *payload_ptr = core_.payloadPool().get(handle);
  if (payload_ptr == nullptr || *payload_ptr == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Command queue handle does not exist");
  }

  auto cb_ref = core_.acquireControlBlock();
  auto cb_handle = cb_ref.handle;
  auto *cb = cb_ref.payload_ptr;
  if (cb == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS command queue control block is unavailable");
  }

  if (!cb->tryBindPayload(handle, payload_ptr, &core_.payloadPool())) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS command queue control block binding failed");
  }
  return CommandQueueLease{cb, core_.controlBlockPoolForLease(), cb_handle};
}

} // namespace orteaf::internal::execution::mps::manager

#endif // ORTEAF_ENABLE_MPS

#include <orteaf/internal/execution/mps/manager/mps_compute_pipeline_state_manager.h>

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::execution::mps::manager {

void MpsComputePipelineStateManager::configure(const Config &config) {
  shutdown();
  if (config.device == nullptr || config.library == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS compute pipeline state manager requires a valid device and "
        "library");
  }
  if (config.ops == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS compute pipeline state manager requires valid ops");
  }
  const std::size_t payload_capacity =
      config.payload_capacity != 0 ? config.payload_capacity : 0u;
  const std::size_t control_block_capacity = config.control_block_capacity != 0
                                                 ? config.control_block_capacity
                                                 : payload_capacity;
  if (payload_capacity >
      static_cast<std::size_t>(FunctionHandle::invalid_index())) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS compute pipeline state manager capacity exceeds maximum handle "
        "range");
  }
  if (control_block_capacity >
      static_cast<std::size_t>(Core::ControlBlockHandle::invalid_index())) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS compute pipeline state manager control block capacity exceeds maximum handle range");
  }
  if (config.payload_growth_chunk_size == 0) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS compute pipeline state manager requires non-zero payload growth chunk size");
  }
  if (config.control_block_growth_chunk_size == 0) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS compute pipeline state manager requires non-zero control block growth chunk size");
  }
  std::size_t payload_block_size = config.payload_block_size;
  if (payload_block_size == 0) {
    payload_block_size = payload_capacity == 0 ? 1u : payload_capacity;
  }
  std::size_t control_block_block_size = config.control_block_block_size;
  if (control_block_block_size == 0) {
    control_block_block_size =
        control_block_capacity == 0 ? 1u : control_block_capacity;
  }

  device_ = config.device;
  library_ = config.library;
  ops_ = config.ops;
  payload_block_size_ = payload_block_size;
  payload_growth_chunk_size_ = config.payload_growth_chunk_size;
  key_to_index_.clear();

  core_.configurePayloadPool(
      PipelinePayloadPool::Config{payload_capacity, payload_block_size_});
  core_.configure(MpsComputePipelineStateManager::Core::Config{
      /*control_block_capacity=*/control_block_capacity,
      /*control_block_block_size=*/control_block_block_size,
      /*growth_chunk_size=*/config.control_block_growth_chunk_size});
  core_.setConfigured(true);
}

void MpsComputePipelineStateManager::shutdown() {
  if (!core_.isConfigured()) {
    return;
  }

  core_.checkCanShutdownOrThrow();
  const PipelinePayloadPoolTraits::Request payload_request{};
  const auto payload_context = makePayloadContext();
  core_.shutdownPayloadPool(payload_request, payload_context);
  core_.shutdownControlBlockPool();
  key_to_index_.clear();
  device_ = nullptr;
  library_ = nullptr;
  ops_ = nullptr;
}

MpsComputePipelineStateManager::PipelineLease
MpsComputePipelineStateManager::acquire(const FunctionKey &key) {
  core_.ensureConfigured();
  validateKey(key);

  // Check cache first
  if (auto it = key_to_index_.find(key); it != key_to_index_.end()) {
    auto handle =
        FunctionHandle{static_cast<FunctionHandle::index_type>(it->second)};
    if (!core_.isAlive(handle)) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "MPS compute pipeline state cache is invalid");
    }
    return core_.acquireWeakLease(handle);
  }

  // Reserve an uncreated slot and create the pipeline state
  PipelinePayloadPoolTraits::Request request{key};
  const auto context = makePayloadContext();
  auto handle = core_.reserveUncreatedPayloadOrGrow(payload_growth_chunk_size_);
  if (!handle.isValid()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::OutOfRange,
        "MPS compute pipeline state manager has no available slots");
  }
  if (!core_.emplacePayload(handle, request, context)) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "Failed to create MPS compute pipeline state");
  }

  key_to_index_.emplace(key, static_cast<std::size_t>(handle.index));
  return core_.acquireWeakLease(handle);
}

void MpsComputePipelineStateManager::validateKey(const FunctionKey &key) const {
  if (key.identifier.empty()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Function identifier cannot be empty");
  }
}

PipelinePayloadPoolTraits::Context
MpsComputePipelineStateManager::makePayloadContext() const noexcept {
  return PipelinePayloadPoolTraits::Context{device_, library_, ops_};
}

} // namespace orteaf::internal::execution::mps::manager

#endif // ORTEAF_ENABLE_MPS

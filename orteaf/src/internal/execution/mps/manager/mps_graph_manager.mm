#include "orteaf/internal/execution/mps/manager/mps_graph_manager.h"

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::execution::mps::manager {

void MpsGraphManager::configure(const Config &config) {
  shutdown();
  if (config.device == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS graph manager requires a valid device");
  }
  if (config.ops == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS graph manager requires valid ops");
  }
  const std::size_t payload_capacity =
      config.payload_capacity != 0 ? config.payload_capacity : 0u;
  const std::size_t control_block_capacity =
      config.control_block_capacity != 0 ? config.control_block_capacity
                                         : payload_capacity;
  if (payload_capacity >
      static_cast<std::size_t>(GraphHandle::invalid_index())) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS graph manager capacity exceeds maximum handle range");
  }
  if (control_block_capacity >
      static_cast<std::size_t>(Core::ControlBlockHandle::invalid_index())) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS graph manager control block capacity exceeds maximum handle range");
  }
  if (config.payload_growth_chunk_size == 0) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS graph manager requires non-zero payload growth chunk size");
  }
  if (config.control_block_growth_chunk_size == 0) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS graph manager requires non-zero control block growth chunk size");
  }

  std::size_t payload_block_size = config.payload_block_size;
  if (payload_block_size == 0) {
    payload_block_size =
        payload_capacity == 0 ? 1u : payload_capacity;
  }
  std::size_t control_block_block_size = config.control_block_block_size;
  if (control_block_block_size == 0) {
    control_block_block_size =
        control_block_capacity == 0 ? 1u : control_block_capacity;
  }

  device_ = config.device;
  ops_ = config.ops;
  payload_block_size_ = payload_block_size;
  payload_growth_chunk_size_ = config.payload_growth_chunk_size;
  key_to_index_.clear();
  next_index_ = 0;

  core_.payloadPool().configure(
      GraphPayloadPool::Config{payload_capacity, payload_block_size_});
  core_.configure(MpsGraphManager::Core::Config{
      /*control_block_capacity=*/control_block_capacity,
      /*control_block_block_size=*/control_block_block_size,
      /*growth_chunk_size=*/config.control_block_growth_chunk_size});
  core_.setInitialized(true);
}

void MpsGraphManager::shutdown() {
  if (!core_.isInitialized()) {
    return;
  }
  core_.checkCanShutdownOrThrow();
  const GraphPayloadPoolTraits::Request payload_request{};
  const auto payload_context = makePayloadContext();
  core_.payloadPool().shutdown(payload_request, payload_context);
  core_.shutdownControlBlockPool();
  key_to_index_.clear();
  next_index_ = 0;
  device_ = nullptr;
  ops_ = nullptr;
}

MpsGraphManager::GraphLease
MpsGraphManager::acquire(const GraphKey &key, const CompileFn &compile_fn) {
  core_.ensureInitialized();
  validateKey(key);
  if (!compile_fn) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS graph compile function cannot be empty");
  }

  // Check if already cached
  if (auto it = key_to_index_.find(key); it != key_to_index_.end()) {
    const auto handle = GraphHandle{
        static_cast<typename GraphHandle::index_type>(it->second)};
    auto *payload_ptr = core_.payloadPool().get(handle);
    if (payload_ptr == nullptr || !core_.payloadPool().isCreated(handle)) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "MPS graph cache is invalid");
    }
    return buildLease(handle, payload_ptr);
  }

  if (next_index_ >= core_.payloadPool().size()) {
    core_.growPayloadPoolBy(payload_growth_chunk_size_);
  }
  if (next_index_ >= core_.payloadPool().size()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::OutOfRange,
        "MPS graph manager has no available slots");
  }

  const auto handle = GraphHandle{
      static_cast<typename GraphHandle::index_type>(next_index_)};
  ++next_index_;
  GraphPayloadPoolTraits::Request request{&compile_fn};
  const auto context = makePayloadContext();
  if (!core_.payloadPool().emplace(handle, request, context)) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "Failed to create MPS graph executable");
  }

  key_to_index_.emplace(key, static_cast<std::size_t>(handle.index));
  auto *payload_ptr = core_.payloadPool().get(handle);
  if (payload_ptr == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS graph payload is unavailable");
  }
  return buildLease(handle, payload_ptr);
}

void MpsGraphManager::validateKey(const GraphKey &key) const {
  if (key.identifier.empty()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Graph identifier cannot be empty");
  }
  if (key.data_type == ::orteaf::internal::execution::mps::platform::wrapper::
                           MpsGraphDataType::kInvalid) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Graph data type must be valid");
  }
  if (key.target_tensor_count == 0) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Graph target tensor count must be > 0");
  }
}

MpsGraphManager::GraphLease
MpsGraphManager::buildLease(GraphHandle handle,
                            MpsGraphResource *payload_ptr) {
  auto cb_ref = core_.acquireControlBlock();
  auto *cb = cb_ref.payload_ptr;
  if (cb == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS graph control block is unavailable");
  }
  if (cb->hasPayload()) {
    if (cb->payloadHandle() != handle) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "MPS graph control block payload mismatch");
    }
  } else if (!cb->tryBindPayload(handle, payload_ptr, &core_.payloadPool())) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS graph control block binding failed");
  }
  return GraphLease{cb, core_.controlBlockPoolForLease(), cb_ref.handle};
}

GraphPayloadPoolTraits::Context
MpsGraphManager::makePayloadContext() const noexcept {
  return GraphPayloadPoolTraits::Context{device_, ops_};
}

} // namespace orteaf::internal::execution::mps::manager

#endif // ORTEAF_ENABLE_MPS

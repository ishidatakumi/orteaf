#include "orteaf/internal/execution/mps/manager/mps_library_manager.h"

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::execution::mps::manager {

void MpsLibraryManager::configure(const Config &config) {
  shutdown();
  if (config.device == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS library manager requires a valid device");
  }
  if (config.ops == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS library manager requires valid ops");
  }
  const std::size_t payload_capacity =
      config.payload_capacity != 0 ? config.payload_capacity : 0u;
  const std::size_t control_block_capacity =
      config.control_block_capacity != 0 ? config.control_block_capacity
                                                 : payload_capacity;
  if (payload_capacity >
      static_cast<std::size_t>(LibraryHandle::invalid_index())) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS library manager capacity exceeds maximum handle range");
  }
  if (control_block_capacity >
      static_cast<std::size_t>(Core::ControlBlockHandle::invalid_index())) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS library manager control block capacity exceeds maximum handle range");
  }
  if (config.payload_growth_chunk_size == 0) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS library manager requires non-zero payload growth chunk size");
  }
  if (config.control_block_growth_chunk_size == 0) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS library manager requires non-zero control block growth chunk size");
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
  ops_ = config.ops;
  pipeline_config_ = config.pipeline_config;
  payload_block_size_ = payload_block_size;
  payload_growth_chunk_size_ = config.payload_growth_chunk_size;
  key_to_index_.clear();

  core_.payloadPool().configure(
      LibraryPayloadPool::Config{payload_capacity, payload_block_size_});
  core_.configure(MpsLibraryManager::Core::Config{
      /*control_block_capacity=*/control_block_capacity,
      /*control_block_block_size=*/control_block_block_size,
      /*growth_chunk_size=*/config.control_block_growth_chunk_size});
  core_.setInitialized(true);
}

void MpsLibraryManager::shutdown() {
  if (!core_.isInitialized()) {
    return;
  }

  core_.checkCanShutdownOrThrow();
  const LibraryPayloadPoolTraits::Request payload_request{};
  const auto payload_context = makePayloadContext();
  core_.payloadPool().shutdown(payload_request, payload_context);
  core_.shutdownControlBlockPool();

  key_to_index_.clear();
  device_ = nullptr;
  ops_ = nullptr;
}

MpsLibraryManager::LibraryLease
MpsLibraryManager::acquire(const LibraryKey &key) {
  core_.ensureInitialized();
  validateKey(key);

  // Check cache first
  if (auto it = key_to_index_.find(key); it != key_to_index_.end()) {
    const auto handle = LibraryHandle{
        static_cast<typename LibraryHandle::index_type>(it->second)};
    auto *payload_ptr =
        const_cast<MpsLibraryResource *>(core_.payloadPool().get(handle));
    if (payload_ptr == nullptr || !core_.payloadPool().isCreated(handle)) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "MPS library cache is invalid");
    }
    return buildLease(handle, payload_ptr);
  }

  // Reserve an uncreated slot and create the library
  LibraryPayloadPoolTraits::Request request{key};
  const auto context = makePayloadContext();
  auto payload_ref = core_.reserveUncreatedPayloadOrGrow(
      payload_growth_chunk_size_, request, context);
  if (!payload_ref.valid()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::OutOfRange,
        "MPS library manager has no available slots");
  }
  const auto handle = payload_ref.handle;
  if (!core_.payloadPool().emplace(handle, request, context)) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "Failed to create MPS library");
  }

  key_to_index_.emplace(key, static_cast<std::size_t>(handle.index));
  auto *payload_ptr =
      const_cast<MpsLibraryResource *>(core_.payloadPool().get(handle));
  if (payload_ptr == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS library payload is unavailable");
  }
  return buildLease(handle, payload_ptr);
}

MpsLibraryManager::LibraryLease
MpsLibraryManager::acquire(LibraryHandle handle) {
  core_.ensureInitialized();
  if (!handle.isValid()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Invalid library handle");
  }
  auto *payload_ptr = core_.payloadPool().get(handle);
  if (payload_ptr == nullptr || payload_ptr->library == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Library handle does not exist");
  }
  return buildLease(handle, payload_ptr);
}

void MpsLibraryManager::validateKey(const LibraryKey &key) const {
  if (key.identifier.empty()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Library identifier cannot be empty");
  }
}

MpsLibraryManager::LibraryLease
MpsLibraryManager::buildLease(LibraryHandle handle,
                              MpsLibraryResource *payload_ptr) {
  auto cb_ref = core_.acquireControlBlock();
  auto *cb = cb_ref.payload_ptr;
  if (cb == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS library control block is unavailable");
  }
  if (cb->hasPayload()) {
    if (cb->payloadHandle() != handle) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "MPS library control block payload mismatch");
    }
  } else if (!cb->tryBindPayload(handle, payload_ptr, &core_.payloadPool())) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS library control block binding failed");
  }
  return LibraryLease{cb, core_.controlBlockPoolForLease(), cb_ref.handle};
}

LibraryPayloadPoolTraits::Context
MpsLibraryManager::makePayloadContext() const noexcept {
  LibraryPayloadPoolTraits::Context context{};
  context.device = device_;
  context.ops = ops_;
  context.pipeline_config = pipeline_config_;
  return context;
}

} // namespace orteaf::internal::execution::mps::manager

#endif // ORTEAF_ENABLE_MPS

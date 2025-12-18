#include <orteaf/internal/runtime/mps/manager/mps_compute_pipeline_state_manager.h>

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::runtime::mps::manager {

void MpsComputePipelineStateManager::initialize(DeviceType device,
                                                LibraryType library,
                                                SlowOps *ops,
                                                std::size_t capacity) {
  if (isInitialized()) {
    shutdown();
  }
  if (device == nullptr || library == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS compute pipeline state manager requires a valid device and "
        "library");
  }
  if (ops == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS compute pipeline state manager requires valid ops");
  }
  if (capacity > static_cast<std::size_t>(FunctionHandle::invalid_index())) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS compute pipeline state manager capacity exceeds maximum handle "
        "range");
  }

  device_ = device;
  library_ = library;
  ops_ = ops;
  key_to_index_.clear();

  // Setup empty pool (cache pattern - grows on demand)
  setupPool(capacity);
}

void MpsComputePipelineStateManager::shutdown() {
  if (!isInitialized()) {
    return;
  }

  // Destroy all created resources
  teardownPool([this](MpsPipelineResource &payload) {
    if (payload.pipeline_state != nullptr) {
      destroyResource(payload);
    }
  });

  key_to_index_.clear();
  device_ = nullptr;
  library_ = nullptr;
  ops_ = nullptr;
}

MpsComputePipelineStateManager::PipelineLease
MpsComputePipelineStateManager::acquire(const FunctionKey &key) {
  ensureInitialized();
  validateKey(key);

  // Check cache first
  if (auto it = key_to_index_.find(key); it != key_to_index_.end()) {
    auto handle = static_cast<FunctionHandle>(it->second);
    auto &cb = Base::acquireExisting(handle); // Increment weak reference count
    return PipelineLease{this, handle, cb.payload().pipeline_state};
  }

  // Acquire new slot and create resource
  Handle handle = acquireFresh([&](MpsPipelineResource &resource) {
    resource.function = ops_->createFunction(library_, key.identifier);
    if (!resource.function)
      return false;

    resource.pipeline_state =
        ops_->createComputePipelineState(device_, resource.function);
    if (!resource.pipeline_state) {
      ops_->destroyFunction(resource.function);
      resource.function = nullptr;
      return false;
    }
    return true;
  });

  if (!handle.isValid()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "Failed to create MPS compute pipeline state");
  }

  key_to_index_.emplace(key, static_cast<std::size_t>(handle.index));
  return PipelineLease{this, handle,
                       getControlBlock(handle).payload().pipeline_state};
}

void MpsComputePipelineStateManager::release(PipelineLease &lease) noexcept {
  if (!lease) {
    return;
  }
  // Decrement weak reference count (resource stays in cache until shutdown)
  Base::releaseForReuse(lease.handle());
  lease.invalidate();
}

void MpsComputePipelineStateManager::validateKey(const FunctionKey &key) const {
  if (key.identifier.empty()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Function identifier cannot be empty");
  }
}

void MpsComputePipelineStateManager::destroyResource(
    MpsPipelineResource &resource) {
  if (resource.pipeline_state != nullptr) {
    ops_->destroyComputePipelineState(resource.pipeline_state);
    resource.pipeline_state = nullptr;
  }
  if (resource.function != nullptr) {
    ops_->destroyFunction(resource.function);
    resource.function = nullptr;
  }
}

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

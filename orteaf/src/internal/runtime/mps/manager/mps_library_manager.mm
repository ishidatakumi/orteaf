#include "orteaf/internal/runtime/mps/manager/mps_library_manager.h"

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::runtime::mps::manager {

void MpsLibraryManager::initialize(DeviceType device, SlowOps *ops,
                                   std::size_t capacity) {
  shutdown();
  if (device == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS library manager requires a valid device");
  }
  if (ops == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS library manager requires valid ops");
  }
  if (capacity > static_cast<std::size_t>(LibraryHandle::invalid_index())) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS library manager capacity exceeds maximum handle range");
  }
  device_ = device;
  ops_ = ops;
  clearCacheStates();
  key_to_index_.clear();
  if (capacity > 0) {
    states_.reserve(capacity);
  }
  initialized_ = true;
}

void MpsLibraryManager::shutdown() {
  if (!initialized_) {
    return;
  }
  for (std::size_t i = 0; i < states_.size(); ++i) {
    State &state = states_[i];
    if (state.alive) {
      state.resource.pipeline_manager.shutdown();
      if (state.resource.library != nullptr) {
        ops_->destroyLibrary(state.resource.library);
        state.resource.library = nullptr;
      }
      state.alive = false;
    }
  }
  clearCacheStates();
  key_to_index_.clear();
  device_ = nullptr;
  ops_ = nullptr;
  initialized_ = false;
}

MpsLibraryManager::LibraryLease
MpsLibraryManager::acquire(const LibraryKey &key) {
  ensureInitialized();
  validateKey(key);

  // Check if already cached
  if (auto it = key_to_index_.find(key); it != key_to_index_.end()) {
    incrementUseCount(it->second);
    return LibraryLease{this, createHandle<LibraryHandle>(it->second),
                        states_[it->second].resource.library};
  }

  // Create new entry
  const std::size_t index = allocateSlot();
  State &state = states_[index];
  state.resource.library = createLibrary(key);
  state.resource.pipeline_manager.initialize(device_, state.resource.library,
                                             ops_, 0);
  markSlotAlive(index);
  key_to_index_.emplace(key, index);

  return LibraryLease{this, createHandle<LibraryHandle>(index),
                      state.resource.library};
}

MpsLibraryManager::LibraryLease
MpsLibraryManager::acquire(LibraryHandle handle) {
  State &state = validateAndGetState(handle);
  incrementUseCount(static_cast<std::size_t>(handle.index));
  return LibraryLease{this, handle, state.resource.library};
}

void MpsLibraryManager::release(LibraryLease &lease) noexcept {
  if (!lease) {
    return;
  }
  decrementUseCount(static_cast<std::size_t>(lease.handle().index));
  lease.invalidate();
}

MpsLibraryManager::PipelineManager *
MpsLibraryManager::pipelineManager(const LibraryLease &lease) {
  State &state = validateAndGetState(lease.handle());
  return &state.resource.pipeline_manager;
}

MpsLibraryManager::PipelineManager *
MpsLibraryManager::pipelineManager(const LibraryKey &key) {
  ensureInitialized();
  validateKey(key);

  std::size_t index = 0;
  State *state = nullptr;

  if (auto it = key_to_index_.find(key); it != key_to_index_.end()) {
    index = it->second;
    state = &states_[index];
  } else {
    index = allocateSlot();
    state = &states_[index];
    state->resource.library = createLibrary(key);
    state->resource.pipeline_manager.initialize(
        device_, state->resource.library, ops_, 0);
    markSlotAlive(index);
    key_to_index_.emplace(key, index);
  }

  return &state->resource.pipeline_manager;
}

void MpsLibraryManager::validateKey(const LibraryKey &key) const {
  if (key.identifier.empty()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Library identifier cannot be empty");
  }
}

::orteaf::internal::runtime::mps::platform::wrapper::MPSLibrary_t
MpsLibraryManager::createLibrary(const LibraryKey &key) {
  switch (key.kind) {
  case LibraryKeyKind::kNamed:
    return ops_->createLibraryWithName(device_, key.identifier);
  }
  ::orteaf::internal::diagnostics::error::throwError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
      "Unsupported MPS library key kind");
}

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

#include "orteaf/internal/runtime/mps/manager/mps_library_manager.h"

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::runtime::mps::manager {

void MpsLibraryManager::initialize(DeviceType device, SlowOps *ops,
                                   std::size_t capacity) {
  if (isInitialized()) {
    shutdown();
  }
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
  key_to_index_.clear();

  // Setup pool (cache pattern - grows on demand)
  setupPool(capacity);
}

void MpsLibraryManager::shutdown() {
  if (!isInitialized()) {
    return;
  }

  // Destroy all created resources
  teardownPool([this](MpsLibraryResource &payload) {
    if (payload.library != nullptr) {
      destroyResource(payload);
    }
  });

  key_to_index_.clear();
  device_ = nullptr;
  ops_ = nullptr;
}

MpsLibraryManager::LibraryLease
MpsLibraryManager::acquire(const LibraryKey &key) {
  ensureInitialized();
  validateKey(key);

  // Check cache first
  if (auto it = key_to_index_.find(key); it != key_to_index_.end()) {
    auto handle = static_cast<LibraryHandle>(it->second);
    auto &cb = Base::acquireExisting(handle); // Increment weak reference count
    return LibraryLease{this, handle, cb.payload().library};
  }

  // Acquire new slot and create resource
  Handle handle = acquireFresh([&](MpsLibraryResource &resource) {
    resource.library = createLibrary(key);
    if (!resource.library)
      return false;

    resource.pipeline_manager.initialize(device_, resource.library, ops_, 0);
    return true;
  });

  if (!handle.isValid()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "Failed to create MPS library");
  }

  key_to_index_.emplace(key, static_cast<std::size_t>(handle.index));
  return LibraryLease{this, handle, getControlBlock(handle).payload().library};
}

MpsLibraryManager::LibraryLease
MpsLibraryManager::acquire(LibraryHandle handle) {
  ensureInitialized();
  auto &cb = Base::acquireExisting(handle); // Increment weak reference count
  return LibraryLease{this, handle, cb.payload().library};
}

void MpsLibraryManager::release(LibraryLease &lease) noexcept {
  if (!lease) {
    return;
  }
  // Decrement weak reference count (resource stays in cache until shutdown)
  Base::releaseForReuse(lease.handle());
  lease.invalidate();
}

MpsLibraryManager::PipelineManager *
MpsLibraryManager::pipelineManager(const LibraryLease &lease) {
  auto &cb = getControlBlockChecked(lease.handle());
  return &cb.payload().pipeline_manager;
}

MpsLibraryManager::PipelineManager *
MpsLibraryManager::pipelineManager(const LibraryKey &key) {
  ensureInitialized();
  validateKey(key);

  // Check cache first
  if (auto it = key_to_index_.find(key); it != key_to_index_.end()) {
    auto handle = static_cast<LibraryHandle>(it->second);
    return &getControlBlock(handle).payload().pipeline_manager;
  }

  // Create new
  Handle handle = acquireFresh([&](MpsLibraryResource &resource) {
    resource.library = createLibrary(key);
    if (!resource.library)
      return false;

    resource.pipeline_manager.initialize(device_, resource.library, ops_, 0);
    return true;
  });

  if (!handle.isValid()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "Failed to create MPS library");
  }

  key_to_index_.emplace(key, static_cast<std::size_t>(handle.index));
  return &getControlBlock(handle).payload().pipeline_manager;
}

void MpsLibraryManager::validateKey(const LibraryKey &key) const {
  if (key.identifier.empty()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Library identifier cannot be empty");
  }
}

::orteaf::internal::runtime::mps::platform::wrapper::MpsLibrary_t
MpsLibraryManager::createLibrary(const LibraryKey &key) {
  switch (key.kind) {
  case LibraryKeyKind::kNamed:
    return ops_->createLibraryWithName(device_, key.identifier);
  }
  ::orteaf::internal::diagnostics::error::throwError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
      "Unsupported MPS library key kind");
}

void MpsLibraryManager::destroyResource(MpsLibraryResource &resource) {
  resource.pipeline_manager.shutdown();
  if (resource.library != nullptr) {
    ops_->destroyLibrary(resource.library);
    resource.library = nullptr;
  }
}

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

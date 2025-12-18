#include "orteaf/internal/runtime/mps/manager/mps_heap_manager.h"

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::runtime::mps::manager {

void MpsHeapManager::initialize(
    DeviceType device, ::orteaf::internal::base::DeviceHandle device_handle,
    MpsLibraryManager *library_manager, SlowOps *ops, std::size_t capacity) {
  shutdown();
  if (device == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS heap manager requires a valid device");
  }
  if (ops == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS heap manager requires valid ops");
  }
  if (capacity > static_cast<std::size_t>(HeapHandle::invalid_index())) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS heap manager capacity exceeds maximum handle range");
  }
  device_ = device;
  device_handle_ = device_handle;
  library_manager_ = library_manager;
  ops_ = ops;
  key_to_index_.clear();
  Base::setupPool(capacity);
}

void MpsHeapManager::shutdown() {
  Base::teardownPool(
      [this](MpsHeapResource &payload) { destroyResource(payload); });
  key_to_index_.clear();
  device_ = nullptr;
  device_handle_ = {};
  library_manager_ = nullptr;
  ops_ = nullptr;
}

MpsHeapManager::HeapLease
MpsHeapManager::acquire(const HeapDescriptorKey &key) {
  Base::ensureInitialized();
  validateKey(key);

  // Check if already cached
  if (auto it = key_to_index_.find(key); it != key_to_index_.end()) {
    HeapHandle cached_handle{
        static_cast<typename HeapHandle::index_type>(it->second)};
    Base::getControlBlock(cached_handle).acquire([](auto &) { return true; });
    return HeapLease{this, cached_handle,
                     Base::getControlBlock(cached_handle).payload().heap};
  }

  // Create new entry
  auto handle = Base::acquireFresh([this, &key](MpsHeapResource &resource) {
    resource.heap = createHeap(key);
    // Initialize buffer manager for this heap
    resource.buffer_manager = std::make_unique<BufferManager>();
    BufferManager::Config buf_cfg{}; // Use defaults
    resource.buffer_manager->initialize(device_, device_handle_, resource.heap,
                                        library_manager_, buf_cfg, 0);
    return true;
  });

  if (handle == HeapHandle::invalid()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS heap manager failed to create heap");
  }

  key_to_index_.emplace(key, static_cast<std::size_t>(handle.index));
  return HeapLease{this, handle, Base::getControlBlock(handle).payload().heap};
}

void MpsHeapManager::release(HeapLease &lease) noexcept {
  if (!lease) {
    return;
  }
  Base::releaseForReuse(lease.handle());
  lease.invalidate();
}

MpsHeapManager::BufferManager *
MpsHeapManager::bufferManager(const HeapLease &lease) {
  if (!lease) {
    return nullptr;
  }
  auto &cb = Base::getControlBlockChecked(lease.handle());
  return cb.payload().buffer_manager.get();
}

MpsHeapManager::BufferManager *
MpsHeapManager::bufferManager(const HeapDescriptorKey &key) {
  Base::ensureInitialized();
  validateKey(key);

  // Check if already cached
  if (auto it = key_to_index_.find(key); it != key_to_index_.end()) {
    HeapHandle cached_handle{
        static_cast<typename HeapHandle::index_type>(it->second)};
    return Base::getControlBlock(cached_handle).payload().buffer_manager.get();
  }

  // Create new entry (same as acquire but don't return lease)
  auto handle = Base::acquireFresh([this, &key](MpsHeapResource &resource) {
    resource.heap = createHeap(key);
    resource.buffer_manager = std::make_unique<BufferManager>();
    BufferManager::Config buf_cfg{};
    resource.buffer_manager->initialize(device_, device_handle_, resource.heap,
                                        library_manager_, buf_cfg, 0);
    return true;
  });

  if (handle == HeapHandle::invalid()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS heap manager failed to create heap");
  }

  key_to_index_.emplace(key, static_cast<std::size_t>(handle.index));
  return Base::getControlBlock(handle).payload().buffer_manager.get();
}

void MpsHeapManager::validateKey(const HeapDescriptorKey &key) const {
  if (key.size_bytes == 0) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Heap size must be > 0");
  }
}

void MpsHeapManager::destroyResource(MpsHeapResource &resource) {
  // Shutdown buffer manager first
  if (resource.buffer_manager) {
    resource.buffer_manager->shutdown();
    resource.buffer_manager.reset();
  }
  // Then destroy heap
  if (resource.heap != nullptr) {
    ops_->destroyHeap(resource.heap);
    resource.heap = nullptr;
  }
}

::orteaf::internal::runtime::mps::platform::wrapper::MpsHeap_t
MpsHeapManager::createHeap(const HeapDescriptorKey &key) {
  auto descriptor = ops_->createHeapDescriptor();
  if (descriptor == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "Failed to allocate MPS heap descriptor");
  }
  struct DescriptorGuard {
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapDescriptor_t
        handle{nullptr};
    SlowOps *slow_ops{nullptr};
    ~DescriptorGuard() {
      if (handle != nullptr && slow_ops != nullptr) {
        slow_ops->destroyHeapDescriptor(handle);
      }
    }
  };
  DescriptorGuard guard{descriptor, ops_};
  ops_->setHeapDescriptorSize(descriptor, key.size_bytes);
  ops_->setHeapDescriptorResourceOptions(descriptor, key.resource_options);
  ops_->setHeapDescriptorStorageMode(descriptor, key.storage_mode);
  ops_->setHeapDescriptorCPUCacheMode(descriptor, key.cpu_cache_mode);
  ops_->setHeapDescriptorHazardTrackingMode(descriptor,
                                            key.hazard_tracking_mode);
  ops_->setHeapDescriptorType(descriptor, key.heap_type);
  auto heap = ops_->createHeap(device_, descriptor);
  ops_->destroyHeapDescriptor(descriptor);
  guard.handle = nullptr;
  if (heap == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "Failed to create MPS heap");
  }
  return heap;
}

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

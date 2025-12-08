#include "orteaf/internal/runtime/mps/manager/mps_heap_manager.h"

#if ORTEAF_ENABLE_MPS

namespace orteaf::internal::runtime::mps::manager {

void MpsHeapManager::initialize(
  ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t device, SlowOps *slow_ops,
    std::size_t capacity) {
  shutdown();
  if (device == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS heap manager requires a valid device");
  }
  if (slow_ops == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS heap manager requires valid ops");
  }
  if (capacity > ::orteaf::internal::base::HeapHandle::invalid_index()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Requested MPS heap capacity exceeds supported limit");
  }
  device_ = device;
  ops_ = slow_ops;
  states_.clear();
  free_list_.clear();
  key_to_index_.clear();
  
  if (capacity > 0) {
    growPool(capacity);
  }
  initialized_ = true;
}

void MpsHeapManager::shutdown() {
  if (!initialized_) {
    return;
  }
  for (std::size_t i = 0; i < states_.size(); ++i) {
    State &state = states_[i];
    if (state.alive) {
      ops_->destroyHeap(state.heap);
      state.reset();
    }
  }
  states_.clear();
  free_list_.clear();
  key_to_index_.clear();
  device_ = nullptr;
  ops_ = nullptr;
  initialized_ = false;
}

MpsHeapManager::HeapLease MpsHeapManager::acquire(const HeapDescriptorKey &key) {
  ensureInitialized();
  validateKey(key);
  if (auto it = key_to_index_.find(key); it != key_to_index_.end()) {
    State &state = states_[it->second];
    if (state.in_use) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "MPS heap is already in use");
    }
    state.in_use = true;
    const auto handle = ::orteaf::internal::base::HeapHandle{static_cast<std::uint32_t>(it->second),
                                     static_cast<::orteaf::internal::base::HeapHandle::generation_type>(state.generation)};
    return HeapLease{this, handle, state.heap};
  }
  const std::size_t index = allocateSlot();
  State &state = states_[index];
  state.key = key;
  state.heap = createHeap(key);
  state.alive = true;
  state.in_use = true;
  const auto handle = ::orteaf::internal::base::HeapHandle{static_cast<std::uint32_t>(index),
                                   static_cast<::orteaf::internal::base::HeapHandle::generation_type>(state.generation)};
  key_to_index_.emplace(state.key, index);
  return HeapLease{this, handle, state.heap};
}

void MpsHeapManager::release(HeapLease &lease) noexcept {
  if (!initialized_ || device_ == nullptr || ops_ == nullptr || !lease) {
    return;
  }
  const auto handle = lease.handle();
  const std::size_t index = static_cast<std::size_t>(handle.index);
  if (index >= states_.size()) {
    return;
  }
  State &state = states_[index];
  if (!state.alive || !state.in_use) {
    return;
  }
  if (static_cast<::orteaf::internal::base::HeapHandle::generation_type>(state.generation) != handle.generation) {
    return;
  }
  state.in_use = false;
  ++state.generation;
  lease.invalidate();
}

#if ORTEAF_ENABLE_TEST
MpsHeapManager::DebugState MpsHeapManager::debugState(::orteaf::internal::base::HeapHandle handle) const {
  DebugState snapshot{};
  snapshot.growth_chunk_size = growth_chunk_size_;
  const std::size_t index = static_cast<std::size_t>(handle.index);
  if (index < states_.size()) {
    const State &state = states_[index];
    snapshot.alive = state.alive;
    snapshot.heap_allocated = state.heap != nullptr;
    snapshot.generation = state.generation;
    snapshot.size_bytes = state.key.size_bytes;
    snapshot.resource_options = state.key.resource_options;
    snapshot.storage_mode = state.key.storage_mode;
    snapshot.cpu_cache_mode = state.key.cpu_cache_mode;
    snapshot.hazard_tracking_mode = state.key.hazard_tracking_mode;
    snapshot.heap_type = state.key.heap_type;
  } else {
    snapshot.generation = std::numeric_limits<std::uint32_t>::max();
  }
  return snapshot;
}
#endif

void MpsHeapManager::validateKey(const HeapDescriptorKey &key) const {
  if (key.size_bytes == 0) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Heap size must be > 0");
  }
}

MpsHeapManager::State &MpsHeapManager::ensureAliveState(::orteaf::internal::base::HeapHandle handle) {
  ensureInitialized();
  const std::size_t index = static_cast<std::size_t>(handle.index);
  if (index >= states_.size()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS heap id out of range");
  }
  State &state = states_[index];
  if (!state.alive) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS heap handle is inactive");
  }
  if (static_cast<::orteaf::internal::base::HeapHandle::generation_type>(state.generation) != handle.generation) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS heap handle is stale");
  }
  return state;
}

::orteaf::internal::runtime::mps::platform::wrapper::MPSHeap_t
MpsHeapManager::createHeap(const HeapDescriptorKey &key) {
  auto descriptor = ops_->createHeapDescriptor();
  if (descriptor == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "Failed to allocate MPS heap descriptor");
  }
  struct DescriptorGuard {
    ::orteaf::internal::runtime::mps::platform::wrapper::MPSHeapDescriptor_t handle{nullptr};
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

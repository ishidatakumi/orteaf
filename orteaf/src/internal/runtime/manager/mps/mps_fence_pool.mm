#include "orteaf/internal/runtime/manager/mps/mps_fence_pool.h"

#if ORTEAF_ENABLE_MPS

namespace orteaf::internal::runtime::mps {

void MpsFencePool::initialize(
    ::orteaf::internal::backend::mps::MPSDevice_t device, BackendOps *ops,
    std::size_t initial_capacity) {
  if (device == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS fence pool requires a valid device");
  }
  if (ops == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS fence pool requires valid ops");
  }
  if (initialized_) {
    shutdown();
  }
  device_ = device;
  ops_ = ops;
  initialized_ = true;
#if ORTEAF_ENABLE_TEST
  total_created_ = 0;
#endif
  free_list_.clear();
  free_list_.reserve(initial_capacity);
  active_count_ = 0;
  if (initial_capacity > 0) {
    growFreeList(initial_capacity);
  }
}

void MpsFencePool::shutdown() {
  if (!initialized_) {
    free_list_.clear();
    active_count_ = 0;
    device_ = nullptr;
    ops_ = nullptr;
#if ORTEAF_ENABLE_TEST
    total_created_ = 0;
#endif
    return;
  }
  if (active_count_ != 0) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "Cannot shutdown MPS fence pool while fences are in use");
  }
  for (std::size_t i = 0; i < free_list_.size(); ++i) {
    ops_->destroyFence(free_list_[i]);
  }
  free_list_.clear();
  active_count_ = 0;
  device_ = nullptr;
  ops_ = nullptr;
#if ORTEAF_ENABLE_TEST
  total_created_ = 0;
#endif
  initialized_ = false;
}

MpsFencePool::Handle MpsFencePool::acquireFence() {
  ensureInitialized();
  if (free_list_.empty()) {
    growFreeList(growth_chunk_size_);
  }
  auto handle = free_list_.back();
  free_list_.resize(free_list_.size() - 1);
  ++active_count_;
  return Handle{this, handle};
}

void MpsFencePool::release(FenceHandle fence) {
  if (fence == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Cannot release null fence to MPS fence pool");
  }
  ensureInitialized();
  if (active_count_ == 0) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "No active fences to release");
  }
  free_list_.pushBack(fence);
  --active_count_;
}

void MpsFencePool::ensureInitialized() const {
  if (!initialized_) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS fence pool has not been initialized");
  }
}

void MpsFencePool::growFreeList(std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    auto handle = ops_->createFence(device_);
    if (handle == nullptr) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::OperationFailed,
          "Backend failed to create MPS fence");
    }
    free_list_.pushBack(handle);
#if ORTEAF_ENABLE_TEST
    ++total_created_;
#endif
  }
}

} // namespace orteaf::internal::runtime::mps

#endif // ORTEAF_ENABLE_MPS

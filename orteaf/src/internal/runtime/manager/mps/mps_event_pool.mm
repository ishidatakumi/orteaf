#include "orteaf/internal/runtime/manager/mps/mps_event_pool.h"

#if ORTEAF_ENABLE_MPS

namespace orteaf::internal::runtime::mps {

void MpsEventPool::initialize(
    ::orteaf::internal::backend::mps::MPSDevice_t device, BackendOps *ops,
    std::size_t initial_capacity) {
  if (device == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS event pool requires a valid device");
  }
  if (ops == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS event pool requires valid ops");
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

void MpsEventPool::shutdown() {
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
        "Cannot shutdown MPS event pool while events are in use");
  }
  for (std::size_t i = 0; i < free_list_.size(); ++i) {
    ops_->destroyEvent(free_list_[i]);
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

MpsEventPool::Handle MpsEventPool::acquireEvent() {
  ensureInitialized();
  if (free_list_.empty()) {
    growFreeList(growth_chunk_size_);
  }
  auto handle = free_list_.back();
  free_list_.resize(free_list_.size() - 1);
  ++active_count_;
  return Handle{this, handle};
}

void MpsEventPool::release(EventHandle event) {
  if (event == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Cannot release null event to MPS event pool");
  }
  ensureInitialized();
  if (active_count_ == 0) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "No active events to release");
  }
  free_list_.pushBack(event);
  --active_count_;
}

void MpsEventPool::ensureInitialized() const {
  if (!initialized_) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS event pool has not been initialized");
  }
}

void MpsEventPool::growFreeList(std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    auto handle = ops_->createEvent(device_);
    if (handle == nullptr) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::OperationFailed,
          "Backend failed to create MPS event");
    }
    free_list_.pushBack(handle);
#if ORTEAF_ENABLE_TEST
    ++total_created_;
#endif
  }
}

} // namespace orteaf::internal::runtime::mps

#endif // ORTEAF_ENABLE_MPS

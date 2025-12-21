#include "orteaf/internal/runtime/mps/manager/mps_device_manager.h"

#if ORTEAF_ENABLE_MPS

namespace orteaf::internal::runtime::mps::manager {

void MpsDeviceManager::initialize(SlowOps *slow_ops) {
  shutdown();

  if (slow_ops == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS device manager requires valid ops");
  }
  ops_ = slow_ops;

  const int device_count = ops_->getDeviceCount();
  if (device_count <= 0) {
    payload_pool_.initialize(DevicePayloadPoolTraits::Config{0});
    control_block_pool_.initialize(DeviceControlBlockPoolTraits::Config{0});
    initialized_ = true;
    return;
  }

  const auto capacity = static_cast<std::size_t>(device_count);
  const auto payload_context = makePayloadContext();
  const DevicePayloadPoolTraits::Request payload_request{};
  payload_pool_.initializeAndCreate(DevicePayloadPoolTraits::Config{capacity},
                                    payload_request, payload_context);

  control_block_pool_.initialize(DeviceControlBlockPoolTraits::Config{capacity});
  initialized_ = true;
}

void MpsDeviceManager::shutdown() {
  if (!initialized_) {
    return;
  }
  if (control_block_pool_.available() != control_block_pool_.capacity()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS device manager shutdown aborted due to active leases");
  }

  const DevicePayloadPoolTraits::Request payload_request{};
  const auto payload_context = makePayloadContext();
  for (std::size_t idx = 0; idx < payload_pool_.capacity(); ++idx) {
    const DeviceHandle handle{static_cast<std::uint32_t>(idx)};
    if (payload_pool_.isCreated(handle)) {
      payload_pool_.destroy(handle, payload_request, payload_context);
    }
  }
  payload_pool_.shutdown();
  control_block_pool_.shutdown();
  ops_ = nullptr;
  initialized_ = false;
}

MpsDeviceManager::DeviceLease MpsDeviceManager::acquire(DeviceHandle handle) {
  ensureInitialized();
  if (!payload_pool_.isValid(handle)) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS device handle is invalid");
  }
  if (!payload_pool_.isCreated(handle)) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS device is unavailable");
  }
  auto *payload_ptr = payload_pool_.get(handle);
  if (payload_ptr == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS device payload is unavailable");
  }

  const DeviceControlBlockPoolTraits::Request cb_request{};
  const DeviceControlBlockPoolTraits::Context cb_context{};
  auto cb_ref = control_block_pool_.tryAcquire(cb_request, cb_context);
  if (!cb_ref.valid()) {
    growControlBlockPool();
    cb_ref = control_block_pool_.tryAcquire(cb_request, cb_context);
  }
  if (!cb_ref.valid()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::OutOfRange,
        "MPS device manager has no available control blocks");
  }

  if (!cb_ref.payload_ptr->tryBindPayload(handle, payload_ptr,
                                          &payload_pool_)) {
    control_block_pool_.release(cb_ref.handle);
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS device control block binding failed");
  }
  return DeviceLease{cb_ref.payload_ptr, &control_block_pool_, cb_ref.handle};
}

::orteaf::internal::architecture::Architecture
MpsDeviceManager::getArch(DeviceHandle handle) const {
  if (!initialized_) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS devices not initialized");
  }
  if (!payload_pool_.isValid(handle)) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS device handle is invalid");
  }
  if (!payload_pool_.isCreated(handle)) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS device is unavailable");
  }
  return payload_pool_.get(handle)->arch;
}

bool MpsDeviceManager::isAlive(DeviceHandle handle) const noexcept {
  return initialized_ && payload_pool_.isValid(handle) &&
         payload_pool_.isCreated(handle);
}

void MpsDeviceManager::ensureInitialized() const {
  if (!initialized_) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS device manager has not been initialized");
  }
}

void MpsDeviceManager::growControlBlockPool() {
  const std::size_t current = control_block_pool_.capacity();
  const std::size_t desired = current + control_block_growth_chunk_;
  const DeviceControlBlockPoolTraits::Request cb_request{};
  const DeviceControlBlockPoolTraits::Context cb_context{};
  control_block_pool_.growAndCreate(
      DeviceControlBlockPoolTraits::Config{desired}, cb_request, cb_context);
}

DevicePayloadPoolTraits::Context
MpsDeviceManager::makePayloadContext() const noexcept {
  return DevicePayloadPoolTraits::Context{
      ops_, command_queue_initial_capacity_, heap_initial_capacity_,
      library_initial_capacity_, graph_initial_capacity_};
}

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

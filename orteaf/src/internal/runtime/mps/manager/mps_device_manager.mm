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
    initialized_ = true;
    return;
  }

  states_.resize(static_cast<std::size_t>(device_count));

  for (int i = 0; i < device_count; ++i) {
    auto &state = states_[i];

    const auto device = ops_->getDevice(
        static_cast<
            ::orteaf::internal::runtime::mps::platform::wrapper::MPSInt_t>(i));
    state.resource.device = device;
    state.alive = device != nullptr;

    const ::orteaf::internal::base::DeviceHandle device_handle{
        static_cast<std::uint32_t>(i)};
    state.resource.arch =
        state.alive
            ? ops_->detectArchitecture(device_handle)
            : ::orteaf::internal::architecture::Architecture::MpsGeneric;

    if (state.alive) {
      state.resource.command_queue_manager.initialize(
          device, ops_, command_queue_initial_capacity_);
      state.resource.library_manager.initialize(device, ops_,
                                                library_initial_capacity_);
      state.resource.heap_manager.initialize(device, device_handle,
                                             &state.resource.library_manager,
                                             ops_, heap_initial_capacity_);
      state.resource.graph_manager.initialize(device, ops_,
                                              graph_initial_capacity_);
      state.resource.event_pool.initialize(device, ops_, 0);
      state.resource.fence_pool.initialize(device, ops_, 0);
      state.use_count = 1; // Mark as active
    }
  }
  initialized_ = true;
}

void MpsDeviceManager::shutdown() {
  if (!initialized_) {
    return;
  }
  for (std::size_t i = 0; i < states_.size(); ++i) {
    states_[i].resource.reset(ops_);
    states_[i].alive = false;
    states_[i].use_count = 0;
  }
  clearCacheStates();
  ops_ = nullptr;
  initialized_ = false;
}

::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t
MpsDeviceManager::device(::orteaf::internal::base::DeviceHandle handle) const {
  return ensureValidStateConst(handle).resource.device;
}

::orteaf::internal::architecture::Architecture
MpsDeviceManager::getArch(::orteaf::internal::base::DeviceHandle handle) const {
  return ensureValidStateConst(handle).resource.arch;
}

bool MpsDeviceManager::isAlive(
    ::orteaf::internal::base::DeviceHandle handle) const {
  if (!initialized_) {
    return false;
  }
  const std::size_t index =
      static_cast<std::size_t>(static_cast<std::uint32_t>(handle));
  if (index >= states_.size()) {
    return false;
  }
  return states_[index].alive;
}

MpsCommandQueueManager *MpsDeviceManager::commandQueueManager(
    ::orteaf::internal::base::DeviceHandle handle) {
  return &ensureValidState(handle).resource.command_queue_manager;
}

MpsHeapManager *
MpsDeviceManager::heapManager(::orteaf::internal::base::DeviceHandle handle) {
  return &ensureValidState(handle).resource.heap_manager;
}

MpsLibraryManager *MpsDeviceManager::libraryManager(
    ::orteaf::internal::base::DeviceHandle handle) {
  return &ensureValidState(handle).resource.library_manager;
}

MpsGraphManager *
MpsDeviceManager::graphManager(::orteaf::internal::base::DeviceHandle handle) {
  return &ensureValidState(handle).resource.graph_manager;
}

MpsEventManager *
MpsDeviceManager::eventPool(::orteaf::internal::base::DeviceHandle handle) {
  return &ensureValidState(handle).resource.event_pool;
}

MpsFenceManager *
MpsDeviceManager::fencePool(::orteaf::internal::base::DeviceHandle handle) {
  return &ensureValidState(handle).resource.fence_pool;
}

MpsDeviceManager::State &MpsDeviceManager::ensureValidState(
    ::orteaf::internal::base::DeviceHandle handle) {
  if (!initialized_) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS devices not initialized");
  }
  const std::size_t index =
      static_cast<std::size_t>(static_cast<std::uint32_t>(handle));
  if (index >= states_.size()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS device handle out of range");
  }
  State &state = states_[index];
  if (!state.alive || state.resource.device == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS device is unavailable");
  }
  return state;
}

const MpsDeviceManager::State &MpsDeviceManager::ensureValidStateConst(
    ::orteaf::internal::base::DeviceHandle handle) const {
  if (!initialized_) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS devices not initialized");
  }
  const std::size_t index =
      static_cast<std::size_t>(static_cast<std::uint32_t>(handle));
  if (index >= states_.size()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS device handle out of range");
  }
  const State &state = states_[index];
  if (!state.alive || state.resource.device == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS device is unavailable");
  }
  return state;
}

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

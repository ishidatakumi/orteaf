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
    Base::setupPoolEmpty();
    return;
  }

  // Setup pool with device count
  Base::setupPool(
      static_cast<std::size_t>(device_count),
      [this](ControlBlock &cb, std::size_t index) {
        const int i = static_cast<int>(index);
        const DeviceHandle handle{static_cast<std::uint32_t>(i)};

        const auto device = ops_->getDevice(
            static_cast<
                ::orteaf::internal::runtime::mps::platform::wrapper::MPSInt_t>(
                i));

        MpsDeviceResource &resource = cb.payload();
        resource.device = device;

        if (device != nullptr) {
          resource.arch = ops_->detectArchitecture(handle);
          resource.command_queue_manager.initialize(
              device, ops_, command_queue_initial_capacity_);
          resource.library_manager.initialize(device, ops_,
                                              library_initial_capacity_);
          resource.heap_manager.initialize(device, handle,
                                           &resource.library_manager, ops_,
                                           heap_initial_capacity_);
          resource.graph_manager.initialize(device, ops_,
                                            graph_initial_capacity_);
          resource.event_pool.initialize(device, ops_, 0);
          resource.fence_pool.initialize(device, ops_, 0);
        } else {
          resource.arch =
              ::orteaf::internal::architecture::Architecture::MpsGeneric;
        }

        return true;
      });
}

void MpsDeviceManager::shutdown() {
  Base::teardownPool(
      [this](ControlBlock &cb, DeviceHandle) { cb.payload().reset(ops_); });
  ops_ = nullptr;
}

::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t
MpsDeviceManager::device(DeviceHandle handle) const {
  return ensureValidControlBlockConst(handle).payload().device;
}

::orteaf::internal::architecture::Architecture
MpsDeviceManager::getArch(DeviceHandle handle) const {
  return ensureValidControlBlockConst(handle).payload().arch;
}

bool MpsDeviceManager::isAlive(DeviceHandle handle) const {
  if (!Base::isInitialized()) {
    return false;
  }
  const std::size_t index = static_cast<std::size_t>(handle.index);
  if (index >= Base::capacity()) {
    return false;
  }
  return Base::getControlBlock(handle).payload().device != nullptr;
}

MpsCommandQueueManager *
MpsDeviceManager::commandQueueManager(DeviceHandle handle) {
  return &ensureValidControlBlock(handle).payload().command_queue_manager;
}

MpsHeapManager *MpsDeviceManager::heapManager(DeviceHandle handle) {
  return &ensureValidControlBlock(handle).payload().heap_manager;
}

MpsLibraryManager *MpsDeviceManager::libraryManager(DeviceHandle handle) {
  return &ensureValidControlBlock(handle).payload().library_manager;
}

MpsGraphManager *MpsDeviceManager::graphManager(DeviceHandle handle) {
  return &ensureValidControlBlock(handle).payload().graph_manager;
}

MpsEventManager *MpsDeviceManager::eventPool(DeviceHandle handle) {
  return &ensureValidControlBlock(handle).payload().event_pool;
}

MpsFenceManager *MpsDeviceManager::fencePool(DeviceHandle handle) {
  return &ensureValidControlBlock(handle).payload().fence_pool;
}

MpsDeviceManager::ControlBlock &
MpsDeviceManager::ensureValidControlBlock(DeviceHandle handle) {
  Base::ensureInitialized();
  const std::size_t index = static_cast<std::size_t>(handle.index);
  if (index >= Base::capacity()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS device handle out of range");
  }
  ControlBlock &cb = Base::getControlBlock(handle);
  if (cb.payload().device == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS device is unavailable");
  }
  return cb;
}

const MpsDeviceManager::ControlBlock &
MpsDeviceManager::ensureValidControlBlockConst(DeviceHandle handle) const {
  if (!Base::isInitialized()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS devices not initialized");
  }
  const std::size_t index = static_cast<std::size_t>(handle.index);
  if (index >= Base::capacity()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS device handle out of range");
  }
  const ControlBlock &cb = Base::getControlBlock(handle);
  if (cb.payload().device == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS device is unavailable");
  }
  return cb;
}

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

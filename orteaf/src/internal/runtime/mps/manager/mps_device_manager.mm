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
    state.reset(ops_);

      const auto device = ops_->getDevice(
        static_cast<::orteaf::internal::runtime::mps::platform::wrapper::MPSInt_t>(i));
    state.device = device;
    state.is_alive = device != nullptr;

    const ::orteaf::internal::base::DeviceHandle device_id{
        static_cast<std::uint32_t>(i)};
    state.arch = state.is_alive
                     ? ops_->detectArchitecture(device_id)
                     : ::orteaf::internal::architecture::Architecture::MpsGeneric;
    if (state.is_alive) {
      state.command_queue_manager.initialize(device, ops_,
                                             command_queue_initial_capacity_);
      state.heap_manager.initialize(device, ops_, heap_initial_capacity_);
      state.library_manager.initialize(device, ops_, library_initial_capacity_);
      state.graph_manager.initialize(device, ops_, graph_initial_capacity_);
      state.event_pool.initialize(device, ops_, 0);
      state.fence_pool.initialize(device, ops_, 0);
    } else {
      state.command_queue_manager.shutdown();
      state.heap_manager.shutdown();
      state.library_manager.shutdown();
      state.graph_manager.shutdown();
      state.event_pool.shutdown();
      state.fence_pool.shutdown();
    }
  }
  initialized_ = true;
}

void MpsDeviceManager::shutdown() {
  if (!initialized_) {
    return;
  }
  for (std::size_t i = 0; i < states_.size(); ++i) {
    states_[i].reset(ops_);
  }
  states_.clear();
  ops_ = nullptr;
  initialized_ = false;
}

MpsDeviceManager::DeviceLease
MpsDeviceManager::acquire(::orteaf::internal::base::DeviceHandle handle) {
  const State& state = ensureValid(handle);
  return DeviceLease{this, state.device};
}

void MpsDeviceManager::release(DeviceLease& lease) noexcept {
  // Devices are owned for the lifetime of the manager; invalidate compatibility lease.
  lease.invalidate();
}

MpsDeviceManager::CommandQueueManagerLease
MpsDeviceManager::acquireCommandQueueManager(::orteaf::internal::base::DeviceHandle handle) {
  State& state = ensureValidState(handle);
  return CommandQueueManagerLease{this, &state.command_queue_manager};
}

void MpsDeviceManager::release(CommandQueueManagerLease& lease) noexcept {
  // Managers live for the device lifetime; invalidate compatibility lease.
  lease.invalidate();
}

MpsDeviceManager::HeapManagerLease
MpsDeviceManager::acquireHeapManager(::orteaf::internal::base::DeviceHandle handle) {
  State& state = ensureValidState(handle);
  return HeapManagerLease{this, &state.heap_manager};
}

void MpsDeviceManager::release(HeapManagerLease& lease) noexcept {
  // Managers live for the device lifetime; invalidate compatibility lease.
  lease.invalidate();
}

MpsDeviceManager::LibraryManagerLease
MpsDeviceManager::acquireLibraryManager(::orteaf::internal::base::DeviceHandle handle) {
  State& state = ensureValidState(handle);
  return LibraryManagerLease{this, &state.library_manager};
}

void MpsDeviceManager::release(LibraryManagerLease& lease) noexcept {
  // Managers live for the device lifetime; invalidate compatibility lease.
  lease.invalidate();
}

MpsDeviceManager::GraphManagerLease
MpsDeviceManager::acquireGraphManager(::orteaf::internal::base::DeviceHandle handle) {
  State& state = ensureValidState(handle);
  return GraphManagerLease{this, &state.graph_manager};
}

void MpsDeviceManager::release(GraphManagerLease& lease) noexcept {
  lease.invalidate();
}

MpsDeviceManager::EventPoolLease
MpsDeviceManager::acquireEventPool(::orteaf::internal::base::DeviceHandle handle) {
  State& state = ensureValidState(handle);
  return EventPoolLease{this, &state.event_pool};
}

void MpsDeviceManager::release(EventPoolLease& lease) noexcept {
  // Pools live for the device lifetime; invalidate compatibility lease.
  lease.invalidate();
}

MpsDeviceManager::FencePoolLease
MpsDeviceManager::acquireFencePool(::orteaf::internal::base::DeviceHandle handle) {
  State& state = ensureValidState(handle);
  return FencePoolLease{this, &state.fence_pool};
}

void MpsDeviceManager::release(FencePoolLease& lease) noexcept {
  // Pools live for the device lifetime; invalidate compatibility lease.
  lease.invalidate();
}

::orteaf::internal::architecture::Architecture
MpsDeviceManager::getArch(::orteaf::internal::base::DeviceHandle handle) const {
  return ensureValid(handle).arch;
}

bool MpsDeviceManager::isAlive(::orteaf::internal::base::DeviceHandle handle) const {
  if (!initialized_) {
    return false;
  }
  const std::size_t index =
      static_cast<std::size_t>(static_cast<std::uint32_t>(handle));
  if (index >= states_.size()) {
    return false;
  }
  return states_[index].is_alive;
}

#if ORTEAF_ENABLE_TEST
MpsDeviceManager::DeviceDebugState
MpsDeviceManager::debugState(::orteaf::internal::base::DeviceHandle handle) const {
  DeviceDebugState debug{};
  const std::size_t index =
      static_cast<std::size_t>(static_cast<std::uint32_t>(handle));
  if (index < states_.size()) {
    debug.in_range = true;
    const auto &state = states_[index];
    debug.is_alive = state.is_alive;
    debug.has_device = state.device != nullptr;
    debug.arch = state.arch;
  }
  return debug;
}
#endif

void MpsDeviceManagerState::reset(SlowOps *slow_ops) noexcept {
  command_queue_manager.shutdown();
  heap_manager.shutdown();
  library_manager.shutdown();
  graph_manager.shutdown();
  event_pool.shutdown();
  fence_pool.shutdown();
  if (device != nullptr && slow_ops != nullptr) {
    slow_ops->releaseDevice(device);
  }
  device = nullptr;
  arch = ::orteaf::internal::architecture::Architecture::MpsGeneric;
  is_alive = false;
}

void MpsDeviceManagerState::moveFrom(MpsDeviceManagerState &&other) noexcept {
  command_queue_manager = std::move(other.command_queue_manager);
  heap_manager = std::move(other.heap_manager);
  library_manager = std::move(other.library_manager);
  graph_manager = std::move(other.graph_manager);
  event_pool = std::move(other.event_pool);
  fence_pool = std::move(other.fence_pool);
  device = other.device;
  arch = other.arch;
  is_alive = other.is_alive;
  other.device = nullptr;
  other.is_alive = false;
}

const MpsDeviceManager::State &
MpsDeviceManager::ensureValid(::orteaf::internal::base::DeviceHandle handle) const {
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
  if (!state.is_alive || state.device == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS device is unavailable");
  }
  return state;
}

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

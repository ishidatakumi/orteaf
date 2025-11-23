#include "orteaf/internal/runtime/manager/mps/mps_device_manager.h"

#if ORTEAF_ENABLE_MPS

namespace orteaf::internal::runtime::mps {

void MpsDeviceManager::initialize(BackendOps *ops) {
  shutdown();

  if (ops == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS device manager requires valid ops");
  }
  ops_ = ops;

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
        static_cast<::orteaf::internal::backend::mps::MPSInt_t>(i));
    state.device = device;
    state.is_alive = device != nullptr;

    const ::orteaf::internal::base::DeviceId device_id{
        static_cast<std::uint32_t>(i)};
    state.arch = state.is_alive
                     ? ops_->detectArchitecture(device_id)
                     : ::orteaf::internal::architecture::Architecture::MpsGeneric;
    if (state.is_alive) {
      state.command_queue_manager.initialize(device, ops_,
                                             command_queue_initial_capacity_);
      state.heap_manager.initialize(device, ops_, heap_initial_capacity_);
      state.library_manager.initialize(device, ops_, library_initial_capacity_);
    } else {
      state.command_queue_manager.shutdown();
      state.heap_manager.shutdown();
      state.library_manager.shutdown();
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

::orteaf::internal::backend::mps::MPSDevice_t
MpsDeviceManager::getDevice(::orteaf::internal::base::DeviceId id) const {
  return ensureValid(id).device;
}

::orteaf::internal::architecture::Architecture
MpsDeviceManager::getArch(::orteaf::internal::base::DeviceId id) const {
  return ensureValid(id).arch;
}

::orteaf::internal::runtime::mps::MpsCommandQueueManager &
MpsDeviceManager::commandQueueManager(::orteaf::internal::base::DeviceId id) {
  return ensureValidState(id).command_queue_manager;
}

const ::orteaf::internal::runtime::mps::MpsCommandQueueManager &
MpsDeviceManager::commandQueueManager(
    ::orteaf::internal::base::DeviceId id) const {
  return ensureValid(id).command_queue_manager;
}

::orteaf::internal::runtime::mps::MpsHeapManager &
MpsDeviceManager::heapManager(::orteaf::internal::base::DeviceId id) {
  return ensureValidState(id).heap_manager;
}

const ::orteaf::internal::runtime::mps::MpsHeapManager &
MpsDeviceManager::heapManager(::orteaf::internal::base::DeviceId id) const {
  return ensureValid(id).heap_manager;
}

::orteaf::internal::runtime::mps::MpsLibraryManager &
MpsDeviceManager::libraryManager(::orteaf::internal::base::DeviceId id) {
  return ensureValidState(id).library_manager;
}

const ::orteaf::internal::runtime::mps::MpsLibraryManager &
MpsDeviceManager::libraryManager(::orteaf::internal::base::DeviceId id) const {
  return ensureValid(id).library_manager;
}

bool MpsDeviceManager::isAlive(::orteaf::internal::base::DeviceId id) const {
  if (!initialized_) {
    return false;
  }
  const std::size_t index =
      static_cast<std::size_t>(static_cast<std::uint32_t>(id));
  if (index >= states_.size()) {
    return false;
  }
  return states_[index].is_alive;
}

#if ORTEAF_ENABLE_TEST
MpsDeviceManager::DeviceDebugState
MpsDeviceManager::debugState(::orteaf::internal::base::DeviceId id) const {
  DeviceDebugState debug{};
  const std::size_t index =
      static_cast<std::size_t>(static_cast<std::uint32_t>(id));
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

void MpsDeviceManager::State::reset(BackendOps *ops) noexcept {
  command_queue_manager.shutdown();
  heap_manager.shutdown();
  library_manager.shutdown();
  if (device != nullptr && ops != nullptr) {
    ops->releaseDevice(device);
  }
  device = nullptr;
  arch = ::orteaf::internal::architecture::Architecture::MpsGeneric;
  is_alive = false;
}

void MpsDeviceManager::State::moveFrom(State &&other) noexcept {
  command_queue_manager = std::move(other.command_queue_manager);
  heap_manager = std::move(other.heap_manager);
  library_manager = std::move(other.library_manager);
  device = other.device;
  arch = other.arch;
  is_alive = other.is_alive;
  other.device = nullptr;
  other.is_alive = false;
}

const MpsDeviceManager::State &
MpsDeviceManager::ensureValid(::orteaf::internal::base::DeviceId id) const {
  if (!initialized_) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS devices not initialized");
  }
  const std::size_t index =
      static_cast<std::size_t>(static_cast<std::uint32_t>(id));
  if (index >= states_.size()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS device id out of range");
  }
  const State &state = states_[index];
  if (!state.is_alive || state.device == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS device is unavailable");
  }
  return state;
}

} // namespace orteaf::internal::runtime::mps

#endif // ORTEAF_ENABLE_MPS

#include "orteaf/internal/execution/mps/manager/mps_device_manager.h"

#if ORTEAF_ENABLE_MPS

namespace orteaf::internal::execution::mps::manager {

void MpsDeviceManager::configure(const Config &config) {
  shutdown();

  if (config.ops == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS device manager requires valid ops");
  }
  ops_ = config.ops;
  core_.setGrowthChunkSize(config.control_block_growth_chunk_size);

  const int device_count = ops_->getDeviceCount();
  const std::size_t device_count_size =
      device_count <= 0 ? 0u : static_cast<std::size_t>(device_count);
  const std::size_t payload_size =
      config.payload_size != 0 ? config.payload_size : device_count_size;
  if (payload_size != device_count_size) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS device manager payload size does not match device count");
  }
  const std::size_t control_block_size =
      config.control_block_size != 0 ? config.control_block_size : payload_size;
  const std::size_t control_block_block_size =
      config.control_block_block_size != 0
          ? config.control_block_block_size
          : (control_block_size == 0 ? 1u : control_block_size);

  auto command_queue_config = config.command_queue_config;
  if (command_queue_config.payload_capacity != 0 &&
      command_queue_config.payload_block_size == 0) {
    command_queue_config.payload_block_size =
        command_queue_config.payload_capacity;
  }
  const auto command_queue_cb_capacity =
      command_queue_config.control_block_capacity != 0
          ? command_queue_config.control_block_capacity
          : command_queue_config.payload_capacity;
  if (command_queue_config.control_block_block_size == 0) {
    command_queue_config.control_block_block_size =
        command_queue_cb_capacity == 0 ? 1u : command_queue_cb_capacity;
  }
  auto event_config = config.event_config;
  if (event_config.payload_capacity != 0 &&
      event_config.payload_block_size == 0) {
    event_config.payload_block_size = event_config.payload_capacity;
  }
  const auto event_cb_capacity = event_config.control_block_capacity != 0
                                     ? event_config.control_block_capacity
                                     : event_config.payload_capacity;
  if (event_config.control_block_block_size == 0) {
    event_config.control_block_block_size =
        event_cb_capacity == 0 ? 1u : event_cb_capacity;
  }
  auto fence_config = config.fence_config;
  if (fence_config.payload_capacity != 0 &&
      fence_config.payload_block_size == 0) {
    fence_config.payload_block_size = fence_config.payload_capacity;
  }
  const auto fence_cb_capacity = fence_config.control_block_capacity != 0
                                     ? fence_config.control_block_capacity
                                     : fence_config.payload_capacity;
  if (fence_config.control_block_block_size == 0) {
    fence_config.control_block_block_size =
        fence_cb_capacity == 0 ? 1u : fence_cb_capacity;
  }
  auto graph_config = config.graph_config;
  if (graph_config.payload_capacity != 0 &&
      graph_config.payload_block_size == 0) {
    graph_config.payload_block_size = graph_config.payload_capacity;
  }
  if (graph_config.control_block_block_size == 0) {
    graph_config.control_block_block_size =
        graph_config.payload_capacity == 0 ? 1u : graph_config.payload_capacity;
  }
  if (device_count <= 0) {
    core_.configurePayloadPool(DevicePayloadPool::Config{0, 0});
    core_.configure(MpsDeviceManager::Core::Config{
        /*control_block_capacity=*/0,
        /*control_block_block_size=*/control_block_block_size,
        /*growth_chunk_size=*/core_.growthChunkSize()});
    core_.setConfigured(true);
    return;
  }

  const auto capacity = device_count_size;
  const auto payload_context =
      DevicePayloadPoolTraits::Context{ops_,
                                       command_queue_config,
                                       event_config,
                                       fence_config,
                                       config.heap_initial_capacity,
                                       config.library_initial_capacity,
                                       graph_config};
  const DevicePayloadPoolTraits::Request payload_request{};
  core_.configurePayloadPool(DevicePayloadPool::Config{capacity, capacity});
  core_.createAllPayloads(payload_request, payload_context);

  core_.configure(MpsDeviceManager::Core::Config{
      /*control_block_capacity=*/control_block_size,
      /*control_block_block_size=*/control_block_block_size,
      /*growth_chunk_size=*/core_.growthChunkSize()});
  core_.setConfigured(true);
}

void MpsDeviceManager::shutdown() {
  if (!core_.isConfigured()) {
    return;
  }
  // Check canShutdown on all created control blocks
  core_.checkCanShutdownOrThrow();

  const DevicePayloadPoolTraits::Request payload_request{};
  const auto payload_context =
      DevicePayloadPoolTraits::Context{ops_,
                                       MpsCommandQueueManager::Config{},
                                       MpsEventManager::Config{},
                                       MpsFenceManager::Config{},
                                       0,
                                       0,
                                       MpsGraphManager::Config{}};
  core_.shutdownPayloadPool(payload_request, payload_context);
  core_.shutdownControlBlockPool();
  ops_ = nullptr;
  core_.setConfigured(false);
}

MpsDeviceManager::DeviceLease MpsDeviceManager::acquire(DeviceHandle handle) {
  return core_.acquireWeakLease(handle);
}

::orteaf::internal::architecture::Architecture
MpsDeviceManager::getArch(DeviceHandle handle) const {
  if (!core_.isConfigured()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS devices not initialized");
  }
  auto lease = core_.acquireWeakLease(handle);
  return lease.payloadPtr()->arch;
}

} // namespace orteaf::internal::execution::mps::manager

#endif // ORTEAF_ENABLE_MPS

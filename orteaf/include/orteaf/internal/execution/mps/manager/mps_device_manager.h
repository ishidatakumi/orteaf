#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>
#include <utility>

#include "orteaf/internal/architecture/architecture.h"
#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/execution/base/lease/control_block/weak.h"
#include "orteaf/internal/execution/base/lease/weak_lease.h"
#include "orteaf/internal/execution/base/manager/base_pool_manager_core.h"
#include "orteaf/internal/execution/base/pool/fixed_slot_store.h"
#include "orteaf/internal/execution/mps/manager/mps_command_queue_manager.h"
#include "orteaf/internal/execution/mps/manager/mps_event_manager.h"
#include "orteaf/internal/execution/mps/manager/mps_fence_manager.h"
#include "orteaf/internal/execution/mps/manager/mps_graph_manager.h"
#include "orteaf/internal/execution/mps/manager/mps_heap_manager.h"
#include "orteaf/internal/execution/mps/manager/mps_library_manager.h"
#include "orteaf/internal/execution/mps/platform/mps_slow_ops.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_device.h"

namespace orteaf::internal::execution::mps::manager {

// =============================================================================
// Device Resource
// =============================================================================

struct MpsDeviceResource {
  using SlowOps = ::orteaf::internal::execution::mps::platform::MpsSlowOps;

  ::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t device{
      nullptr};
  ::orteaf::internal::architecture::Architecture arch{
      ::orteaf::internal::architecture::Architecture::MpsGeneric};
  MpsCommandQueueManager command_queue_manager{};
  MpsHeapManager heap_manager{};
  MpsLibraryManager library_manager{};
  MpsGraphManager graph_manager{};
  MpsEventManager event_pool{};
  MpsFenceManager fence_pool{};

  MpsDeviceResource() = default;
  MpsDeviceResource(const MpsDeviceResource &) = delete;
  MpsDeviceResource &operator=(const MpsDeviceResource &) = delete;

  MpsDeviceResource(MpsDeviceResource &&other) noexcept {
    moveFrom(std::move(other));
  }

  MpsDeviceResource &operator=(MpsDeviceResource &&other) noexcept {
    if (this != &other) {
      reset(nullptr);
      moveFrom(std::move(other));
    }
    return *this;
  }

  ~MpsDeviceResource() { reset(nullptr); }

  void reset(SlowOps *slow_ops) noexcept {
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
  }

private:
  void moveFrom(MpsDeviceResource &&other) noexcept {
    command_queue_manager = std::move(other.command_queue_manager);
    heap_manager = std::move(other.heap_manager);
    library_manager = std::move(other.library_manager);
    graph_manager = std::move(other.graph_manager);
    event_pool = std::move(other.event_pool);
    fence_pool = std::move(other.fence_pool);
    device = other.device;
    arch = other.arch;
    other.device = nullptr;
  }
};

// =============================================================================
// Payload Pool
// =============================================================================

struct DevicePayloadPoolTraits {
  using Payload = MpsDeviceResource;
  using Handle = ::orteaf::internal::base::DeviceHandle;
  using SlowOps = ::orteaf::internal::execution::mps::platform::MpsSlowOps;

  struct Request {
    Handle handle{Handle::invalid()};
  };

  struct Context {
    SlowOps *ops{nullptr};
    MpsCommandQueueManager::Config command_queue_config{};
    MpsEventManager::Config event_config{};
    MpsFenceManager::Config fence_config{};
    std::size_t heap_initial_capacity{0};
    std::size_t library_initial_capacity{0};
    MpsGraphManager::Config graph_config{};
  };

  static bool create(Payload &payload, const Request &request,
                     const Context &context) {
    if (context.ops == nullptr || !request.handle.isValid()) {
      return false;
    }
    const auto device = context.ops->getDevice(
        static_cast<
            ::orteaf::internal::execution::mps::platform::wrapper::MPSInt_t>(
            request.handle.index));
    payload.device = device;
    if (device == nullptr) {
      payload.arch = ::orteaf::internal::architecture::Architecture::MpsGeneric;
      return false;
    }
    payload.arch = context.ops->detectArchitecture(request.handle);
    auto command_queue_config = context.command_queue_config;
    command_queue_config.device = device;
    command_queue_config.ops = context.ops;
    payload.command_queue_manager.configure(command_queue_config);
    // Configure library manager (payload/control capacities share initial
    // value).
    MpsLibraryManager::Config library_config{};
    library_config.device = device;
    library_config.ops = context.ops;
    library_config.payload_capacity = context.library_initial_capacity;
    library_config.control_block_capacity = context.library_initial_capacity;
    payload.library_manager.configure(library_config);
    // Configure heap manager
    MpsHeapManager::Config heap_config{};
    heap_config.device = device;
    heap_config.device_handle = request.handle;
    heap_config.library_manager = &payload.library_manager;
    heap_config.ops = context.ops;
    heap_config.payload_capacity = context.heap_initial_capacity;
    heap_config.control_block_capacity = context.heap_initial_capacity;
    payload.heap_manager.configure(heap_config);
    auto graph_config = context.graph_config;
    graph_config.device = device;
    graph_config.ops = context.ops;
    payload.graph_manager.configure(graph_config);
    auto event_config = context.event_config;
    event_config.device = device;
    event_config.ops = context.ops;
    payload.event_pool.configure(event_config);
    auto fence_config = context.fence_config;
    fence_config.device = device;
    fence_config.ops = context.ops;
    payload.fence_pool.configure(fence_config);
    return true;
  }

  static void destroy(Payload &payload, const Request &,
                      const Context &context) {
    payload.reset(context.ops);
  }
};

using DevicePayloadPool =
    ::orteaf::internal::execution::base::pool::FixedSlotStore<
        DevicePayloadPoolTraits>;

// =============================================================================
// ControlBlock (using default pool traits via BasePoolManagerCore)
// =============================================================================

struct DeviceControlBlockTag {};

using DeviceControlBlock = ::orteaf::internal::execution::base::WeakControlBlock<
    ::orteaf::internal::base::DeviceHandle, MpsDeviceResource,
    DevicePayloadPool>;

// =============================================================================
// Manager Traits for BasePoolManagerCore
// =============================================================================

struct MpsDeviceManagerTraits {
  using PayloadPool = DevicePayloadPool;
  using ControlBlock = DeviceControlBlock;
  struct ControlBlockTag {};
  using PayloadHandle = ::orteaf::internal::base::DeviceHandle;
  static constexpr const char *Name = "MPS device manager";
};

// =============================================================================
// MpsDeviceManager
// =============================================================================

class MpsDeviceManager {
public:
  using SlowOps = ::orteaf::internal::execution::mps::platform::MpsSlowOps;
  using DeviceHandle = ::orteaf::internal::base::DeviceHandle;
  using DeviceType =
      ::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t;

  using Core = ::orteaf::internal::execution::base::BasePoolManagerCore<
      MpsDeviceManagerTraits>;
  using ControlBlock = Core::ControlBlock;
  using ControlBlockHandle = Core::ControlBlockHandle;
  using ControlBlockPool = Core::ControlBlockPool;

  using DeviceLease = ::orteaf::internal::execution::base::WeakLease<
      ControlBlockHandle, ControlBlock, ControlBlockPool, MpsDeviceManager>;

  struct Config {
    SlowOps *ops{nullptr};
    std::size_t payload_size{0};
    std::size_t heap_initial_capacity{0};
    std::size_t library_initial_capacity{0};
    std::size_t control_block_size{0};
    std::size_t control_block_block_size{0};
    std::size_t control_block_growth_chunk_size{1};
    MpsCommandQueueManager::Config command_queue_config{};
    MpsEventManager::Config event_config{};
    MpsFenceManager::Config fence_config{};
    MpsGraphManager::Config graph_config{};
  };

  MpsDeviceManager() = default;
  MpsDeviceManager(const MpsDeviceManager &) = delete;
  MpsDeviceManager &operator=(const MpsDeviceManager &) = delete;
  MpsDeviceManager(MpsDeviceManager &&) = default;
  MpsDeviceManager &operator=(MpsDeviceManager &&) = default;
  ~MpsDeviceManager() = default;

  // =========================================================================
  // Lifecycle
  // =========================================================================
  void configure(const Config &config);
  void shutdown();

  // =========================================================================
  // Device access
  // =========================================================================
  DeviceLease acquire(DeviceHandle handle);
  void release(DeviceLease &lease) noexcept { lease.release(); }

  ::orteaf::internal::architecture::Architecture
  getArch(DeviceHandle handle) const;

#if ORTEAF_ENABLE_TEST
  std::size_t getDeviceCountForTest() const noexcept {
    return core_.payloadPool().size();
  }
  bool isInitializedForTest() const noexcept { return core_.isInitialized(); }
  std::size_t payloadPoolSizeForTest() const noexcept {
    return core_.payloadPool().size();
  }
  std::size_t payloadPoolCapacityForTest() const noexcept {
    return core_.payloadPool().capacity();
  }
  bool isAliveForTest(DeviceHandle handle) const noexcept {
    return core_.isAlive(handle);
  }
  std::size_t controlBlockPoolAvailableForTest() const noexcept {
    return core_.controlBlockPoolAvailableForTest();
  }
#endif

private:
  SlowOps *ops_{nullptr};
  Core core_{};
};

} // namespace orteaf::internal::execution::mps::manager

#endif // ORTEAF_ENABLE_MPS

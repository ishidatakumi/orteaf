#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>
#include <utility>

#include "orteaf/internal/architecture/architecture.h"
#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/runtime/base/lease/control_block/weak.h"
#include "orteaf/internal/runtime/base/lease/weak_lease.h"
#include "orteaf/internal/runtime/base/pool/fixed_slot_store.h"
#include "orteaf/internal/runtime/base/pool/slot_pool.h"
#include "orteaf/internal/runtime/mps/manager/mps_command_queue_manager.h"
#include "orteaf/internal/runtime/mps/manager/mps_event_manager.h"
#include "orteaf/internal/runtime/mps/manager/mps_fence_manager.h"
#include "orteaf/internal/runtime/mps/manager/mps_graph_manager.h"
#include "orteaf/internal/runtime/mps/manager/mps_heap_manager.h"
#include "orteaf/internal/runtime/mps/manager/mps_library_manager.h"
#include "orteaf/internal/runtime/mps/platform/mps_slow_ops.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_device.h"

namespace orteaf::internal::runtime::mps::manager {

// =============================================================================
// Device Resource
// =============================================================================

struct MpsDeviceResource {
  using SlowOps = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;

  ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t device{
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
// Pools (payload + control block)
// =============================================================================

struct DevicePayloadPoolTraits {
  using Payload = MpsDeviceResource;
  using Handle = ::orteaf::internal::base::DeviceHandle;
  using SlowOps = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;

  struct Request {
    Handle handle{Handle::invalid()};
  };

  struct Context {
    SlowOps *ops{nullptr};
    std::size_t command_queue_initial_capacity{0};
    std::size_t heap_initial_capacity{0};
    std::size_t library_initial_capacity{0};
    std::size_t graph_initial_capacity{0};
  };

  struct Config {
    std::size_t capacity{0};
  };

  static bool create(Payload &payload, const Request &request,
                     const Context &context) {
    if (context.ops == nullptr || !request.handle.isValid()) {
      return false;
    }
    const auto device = context.ops->getDevice(
        static_cast<
            ::orteaf::internal::runtime::mps::platform::wrapper::MPSInt_t>(
            request.handle.index));
    payload.device = device;
    if (device == nullptr) {
      payload.arch =
          ::orteaf::internal::architecture::Architecture::MpsGeneric;
      return false;
    }
    payload.arch = context.ops->detectArchitecture(request.handle);
    payload.command_queue_manager.initialize(
        device, context.ops, context.command_queue_initial_capacity);
    payload.library_manager.initialize(device, context.ops,
                                       context.library_initial_capacity);
    payload.heap_manager.initialize(device, request.handle,
                                    &payload.library_manager, context.ops,
                                    context.heap_initial_capacity);
    payload.graph_manager.initialize(device, context.ops,
                                     context.graph_initial_capacity);
    payload.event_pool.initialize(device, context.ops, 0);
    payload.fence_pool.initialize(device, context.ops, 0);
    return true;
  }

  static void destroy(Payload &payload, const Request &,
                      const Context &context) {
    payload.reset(context.ops);
  }
};

using DevicePayloadPool =
    ::orteaf::internal::runtime::base::pool::FixedSlotStore<
        DevicePayloadPoolTraits>;

struct DeviceControlBlockTag {};
using DeviceControlBlockHandle = ::orteaf::internal::base::Handle<
    DeviceControlBlockTag, std::uint32_t, std::uint8_t>;

using DeviceControlBlock = ::orteaf::internal::runtime::base::WeakControlBlock<
    ::orteaf::internal::base::DeviceHandle, MpsDeviceResource,
    DevicePayloadPool>;

struct DeviceControlBlockPoolTraits {
  using Payload = DeviceControlBlock;
  using Handle = DeviceControlBlockHandle;
  struct Request {
    Handle handle{Handle::invalid()};
  };
  struct Context {};
  struct Config {
    std::size_t capacity{0};
  };

  static bool create(Payload &, const Request &, const Context &) {
    return true;
  }

  static void destroy(Payload &, const Request &, const Context &) {}
};

using DeviceControlBlockPool =
    ::orteaf::internal::runtime::base::pool::SlotPool<
        DeviceControlBlockPoolTraits>;

// =============================================================================
// MpsDeviceManager
// =============================================================================

class MpsDeviceManager {
public:
  using SlowOps = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;
  using DeviceHandle = ::orteaf::internal::base::DeviceHandle;
  using DeviceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t;
  using ControlBlock = DeviceControlBlock;
  using ControlBlockHandle = DeviceControlBlockHandle;
  using DeviceLease = ::orteaf::internal::runtime::base::WeakLease<
      ControlBlockHandle, ControlBlock, DeviceControlBlockPool,
      MpsDeviceManager>;

  MpsDeviceManager() = default;
  MpsDeviceManager(const MpsDeviceManager &) = delete;
  MpsDeviceManager &operator=(const MpsDeviceManager &) = delete;
  MpsDeviceManager(MpsDeviceManager &&) = default;
  MpsDeviceManager &operator=(MpsDeviceManager &&) = default;
  ~MpsDeviceManager() = default;

  // =========================================================================
  // Configuration (call before initialize)
  // =========================================================================
  void setCommandQueueInitialCapacity(std::size_t capacity) {
    command_queue_initial_capacity_ = capacity;
  }
  std::size_t commandQueueInitialCapacity() const noexcept {
    return command_queue_initial_capacity_;
  }

  void setHeapInitialCapacity(std::size_t capacity) {
    heap_initial_capacity_ = capacity;
  }
  std::size_t heapInitialCapacity() const noexcept {
    return heap_initial_capacity_;
  }

  void setLibraryInitialCapacity(std::size_t capacity) {
    library_initial_capacity_ = capacity;
  }
  std::size_t libraryInitialCapacity() const noexcept {
    return library_initial_capacity_;
  }

  void setGraphInitialCapacity(std::size_t capacity) {
    graph_initial_capacity_ = capacity;
  }
  std::size_t graphInitialCapacity() const noexcept {
    return graph_initial_capacity_;
  }

  void setControlBlockGrowthChunkSize(std::size_t size) {
    if (size == 0) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          "MPS device manager control block growth chunk must be > 0");
    }
    control_block_growth_chunk_ = size;
  }
  std::size_t controlBlockGrowthChunkSize() const noexcept {
    return control_block_growth_chunk_;
  }

  // =========================================================================
  // Lifecycle
  // =========================================================================
  void initialize(SlowOps *slow_ops);
  void shutdown();

  // =========================================================================
  // Device access
  // =========================================================================
  std::size_t getDeviceCount() const { return payload_pool_.capacity(); }

  DeviceLease acquire(DeviceHandle handle);
  void release(DeviceLease &lease) noexcept { lease.release(); }

  ::orteaf::internal::architecture::Architecture
  getArch(DeviceHandle handle) const;

  bool isInitialized() const noexcept { return initialized_; }
  std::size_t capacity() const noexcept { return payload_pool_.capacity(); }
  bool isAlive(DeviceHandle handle) const noexcept;

#if ORTEAF_ENABLE_TEST
  std::size_t controlBlockPoolAvailableForTest() const noexcept {
    return control_block_pool_.available();
  }
#endif

private:
  void ensureInitialized() const;
  void growControlBlockPool();
  DevicePayloadPoolTraits::Context makePayloadContext() const noexcept;

  SlowOps *ops_{nullptr};
  std::size_t command_queue_initial_capacity_{0};
  std::size_t heap_initial_capacity_{0};
  std::size_t library_initial_capacity_{0};
  std::size_t graph_initial_capacity_{0};
  std::size_t control_block_growth_chunk_{1};
  bool initialized_{false};
  DevicePayloadPool payload_pool_{};
  DeviceControlBlockPool control_block_pool_{};
};

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

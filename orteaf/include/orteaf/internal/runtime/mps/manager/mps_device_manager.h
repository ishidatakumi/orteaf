#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>
#include <utility>

#include "orteaf/internal/architecture/architecture.h"
#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/runtime/base/lease/control_block/raw.h"
#include "orteaf/internal/runtime/base/lease/raw_lease.h"
#include "orteaf/internal/runtime/base/lease/slot.h"
#include "orteaf/internal/runtime/base/manager/base_manager_core.h"
#include "orteaf/internal/runtime/mps/manager/mps_buffer_manager.h"
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
// BaseManagerCore Types (RawControlBlock - no lifecycle tracking)
// =============================================================================

using DeviceSlot =
    ::orteaf::internal::runtime::base::RawSlot<MpsDeviceResource>;
using DeviceControlBlock =
    ::orteaf::internal::runtime::base::RawControlBlock<DeviceSlot>;

struct MpsDeviceManagerTraits {
  using ControlBlock = DeviceControlBlock;
  using Handle = ::orteaf::internal::base::DeviceHandle;
  static constexpr const char *Name = "MpsDeviceManager";
};

// =============================================================================
// MpsDeviceManager
// =============================================================================

class MpsDeviceManager
    : protected ::orteaf::internal::runtime::base::BaseManagerCore<
          MpsDeviceManagerTraits> {
  using Base = ::orteaf::internal::runtime::base::BaseManagerCore<
      MpsDeviceManagerTraits>;

public:
  using SlowOps = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;
  using DeviceHandle = ::orteaf::internal::base::DeviceHandle;
  using DeviceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t;
  using ControlBlock = typename Base::ControlBlock;
  using DeviceLease =
      ::orteaf::internal::runtime::base::RawLease<DeviceHandle, DeviceType,
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

  // =========================================================================
  // Lifecycle
  // =========================================================================
  void initialize(SlowOps *slow_ops);
  void shutdown();

  // =========================================================================
  // Device access
  // =========================================================================
  std::size_t getDeviceCount() const { return Base::capacity(); }

  DeviceLease acquire(DeviceHandle handle);
  void release(DeviceLease &lease) noexcept;

  ::orteaf::internal::architecture::Architecture
  getArch(DeviceHandle handle) const;


  // =========================================================================
  // Direct access to child managers
  // =========================================================================
  MpsCommandQueueManager *commandQueueManager(DeviceHandle handle);
  MpsHeapManager *heapManager(DeviceHandle handle);
  MpsLibraryManager *libraryManager(DeviceHandle handle);
  MpsGraphManager *graphManager(DeviceHandle handle);
  MpsEventManager *eventPool(DeviceHandle handle);
  MpsFenceManager *fencePool(DeviceHandle handle);

  // Expose base methods
  using Base::isAlive;
  using Base::capacity;
  using Base::isInitialized;

#if ORTEAF_ENABLE_TEST
  using Base::controlBlockForTest;
  using Base::freeListSizeForTest;
  using Base::isInitializedForTest;
#endif

private:
  ControlBlock &ensureValidControlBlock(DeviceHandle handle);
  const ControlBlock &ensureValidControlBlockConst(DeviceHandle handle) const;

  SlowOps *ops_{nullptr};
  std::size_t command_queue_initial_capacity_{0};
  std::size_t heap_initial_capacity_{0};
  std::size_t library_initial_capacity_{0};
  std::size_t graph_initial_capacity_{0};
};

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

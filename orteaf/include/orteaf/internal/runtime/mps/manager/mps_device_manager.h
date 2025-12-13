#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>
#include <utility>

#include "orteaf/internal/architecture/architecture.h"
#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/runtime/base/shared_cache_manager.h"
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

// Resource struct: holds device and all child managers
struct MpsDeviceResource {
  using SlowOps = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;

  ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t device{
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

// Use SharedCacheState template
using MpsDeviceManagerState =
    ::orteaf::internal::runtime::base::SharedCacheState<MpsDeviceResource>;

struct MpsDeviceManagerTraits {
  using DeviceType = void *;
  using OpsType = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;
  using StateType = MpsDeviceManagerState;
  static constexpr const char *Name = "MPS device manager";
};

class MpsDeviceManager
    : public ::orteaf::internal::runtime::base::SharedCacheManager<
          MpsDeviceManager, MpsDeviceManagerTraits> {
public:
  using Base = ::orteaf::internal::runtime::base::SharedCacheManager<
      MpsDeviceManager, MpsDeviceManagerTraits>;
  using SlowOps = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;

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
  // Device info
  // =========================================================================
  std::size_t getDeviceCount() const { return states_.size(); }

  ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t
  device(::orteaf::internal::base::DeviceHandle handle) const;

  ::orteaf::internal::architecture::Architecture
  getArch(::orteaf::internal::base::DeviceHandle handle) const;

  bool isAlive(::orteaf::internal::base::DeviceHandle handle) const;

  // =========================================================================
  // Direct access to child managers
  // =========================================================================
  MpsCommandQueueManager *
  commandQueueManager(::orteaf::internal::base::DeviceHandle handle);

  MpsHeapManager *heapManager(::orteaf::internal::base::DeviceHandle handle);

  MpsLibraryManager *
  libraryManager(::orteaf::internal::base::DeviceHandle handle);

  MpsGraphManager *graphManager(::orteaf::internal::base::DeviceHandle handle);

  MpsEventManager *eventPool(::orteaf::internal::base::DeviceHandle handle);

  MpsFenceManager *fencePool(::orteaf::internal::base::DeviceHandle handle);

private:
  State &ensureValidState(::orteaf::internal::base::DeviceHandle handle);
  const State &
  ensureValidStateConst(::orteaf::internal::base::DeviceHandle handle) const;

  std::size_t command_queue_initial_capacity_{0};
  std::size_t heap_initial_capacity_{0};
  std::size_t library_initial_capacity_{0};
  std::size_t graph_initial_capacity_{0};
};

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

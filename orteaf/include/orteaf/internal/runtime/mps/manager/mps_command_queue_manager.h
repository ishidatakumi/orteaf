#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/runtime/base/lease/control_block/unique.h"
#include "orteaf/internal/runtime/base/lease/slot.h"
#include "orteaf/internal/runtime/base/lease/unique_lease.h"
#include "orteaf/internal/runtime/base/manager/base_manager_core.h"
#include "orteaf/internal/runtime/mps/platform/mps_slow_ops.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_command_queue.h"

namespace orteaf::internal::runtime::mps::manager {

// Slot type
using CommandQueueSlot = ::orteaf::internal::runtime::base::GenerationalSlot<
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsCommandQueue_t>;

// Control block: Unique (exclusive) ownership
using CommandQueueControlBlock =
    ::orteaf::internal::runtime::base::UniqueControlBlock<CommandQueueSlot>;

struct MpsCommandQueueManagerTraits {
  using ControlBlock = CommandQueueControlBlock;
  using Handle = ::orteaf::internal::base::CommandQueueHandle;
  static constexpr const char *Name = "MPS command queue manager";
};

class MpsCommandQueueManager
    : protected ::orteaf::internal::runtime::base::BaseManagerCore<
          MpsCommandQueueManagerTraits> {
  using Base = ::orteaf::internal::runtime::base::BaseManagerCore<
      MpsCommandQueueManagerTraits>;

public:
  using SlowOps = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;
  using DeviceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t;
  using CommandQueueHandle = ::orteaf::internal::base::CommandQueueHandle;
  using CommandQueueType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsCommandQueue_t;
  using CommandQueueLease = ::orteaf::internal::runtime::base::UniqueLease<
      CommandQueueHandle, CommandQueueType, MpsCommandQueueManager>;

  MpsCommandQueueManager() = default;
  MpsCommandQueueManager(const MpsCommandQueueManager &) = delete;
  MpsCommandQueueManager &operator=(const MpsCommandQueueManager &) = delete;
  MpsCommandQueueManager(MpsCommandQueueManager &&) = default;
  MpsCommandQueueManager &operator=(MpsCommandQueueManager &&) = default;
  ~MpsCommandQueueManager() = default;

  void initialize(DeviceType device, SlowOps *ops, std::size_t capacity);
  void shutdown();
  void growCapacity(std::size_t additional);

  CommandQueueLease acquire();
  void release(CommandQueueLease &lease) noexcept;
  void release(CommandQueueHandle handle) noexcept;

  bool isInUse(CommandQueueHandle handle) const;
  void releaseUnusedQueues();

  // Config
  using Base::growthChunkSize;
  using Base::setGrowthChunkSize;

  // Expose capacity
  using Base::capacity;
  using Base::isInitialized;

#if ORTEAF_ENABLE_TEST
  using Base::controlBlockForTest;
  using Base::freeListSizeForTest;
  using Base::isInitializedForTest;
#endif

private:
  void destroyResource(CommandQueueType &resource);

  DeviceType device_{nullptr};
  SlowOps *ops_{nullptr};
};

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

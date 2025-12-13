#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/lease.h"
#include "orteaf/internal/runtime/base/exclusive_pool_manager.h"
#include "orteaf/internal/runtime/mps/platform/mps_slow_ops.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_command_queue.h"

namespace orteaf::internal::runtime::mps::manager {

// Use the standard ExclusivePoolState template
using MpsCommandQueueManagerState =
    ::orteaf::internal::runtime::base::ExclusivePoolState<
        ::orteaf::internal::runtime::mps::platform::wrapper::MPSCommandQueue_t>;

struct MpsCommandQueueManagerTraits {
  using OpsType = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;
  using StateType = MpsCommandQueueManagerState;
  static constexpr const char *Name = "MPS command queue manager";
};

class MpsCommandQueueManager
    : public ::orteaf::internal::runtime::base::ExclusivePoolManager<
          MpsCommandQueueManager, MpsCommandQueueManagerTraits> {
public:
  using Base = ::orteaf::internal::runtime::base::ExclusivePoolManager<
      MpsCommandQueueManager, MpsCommandQueueManagerTraits>;
  using SlowOps = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;
  using DeviceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t;
  using CommandQueueHandle = ::orteaf::internal::base::CommandQueueHandle;
  using CommandQueueLease = ::orteaf::internal::base::Lease<
      CommandQueueHandle,
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSCommandQueue_t,
      MpsCommandQueueManager>;

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

  bool isInUse(CommandQueueHandle handle) const;
  void releaseUnusedQueues();

private:
  DeviceType device_{nullptr};
};

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

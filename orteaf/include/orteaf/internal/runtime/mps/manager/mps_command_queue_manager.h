#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/base/lease.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/diagnostics/log/log_config.h"
#include "orteaf/internal/runtime/base/base_manager.h"
#include "orteaf/internal/runtime/mps/platform/mps_slow_ops.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_command_queue.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_event.h"

namespace orteaf::internal::runtime::mps::manager {

/**
 * @brief Stub implementation of an MPS command queue manager.
 *
 * The real implementation will manage a freelist of queues backed by
 * SlowOps. For now we only provide the API surface required by the unit
 * tests so the project builds; behaviour is intentionally minimal.
 */

struct MpsCommandQueueManagerState {
  using SlowOps = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;
  ::orteaf::internal::runtime::mps::platform::wrapper::MPSCommandQueue_t
      command_queue{nullptr};
  bool in_use{false};
  std::uint32_t generation{0};

  void destroy(SlowOps *slow_ops) noexcept;
};

struct MpsCommandQueueManagerTraits {
  using DeviceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t;
  using OpsType = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;
  using StateType = MpsCommandQueueManagerState;
  static constexpr const char *Name = "MPS command queue manager";
};

class MpsCommandQueueManager
    : public base::BaseManager<MpsCommandQueueManager,
                               MpsCommandQueueManagerTraits> {
public:
  using SlowOps = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;
  using CommandQueueLease = ::orteaf::internal::base::Lease<
      ::orteaf::internal::base::CommandQueueHandle,
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSCommandQueue_t,
      MpsCommandQueueManager>;

  MpsCommandQueueManager() = default;
  MpsCommandQueueManager(const MpsCommandQueueManager &) = delete;
  MpsCommandQueueManager &operator=(const MpsCommandQueueManager &) = delete;
  MpsCommandQueueManager(MpsCommandQueueManager &&) = default;
  MpsCommandQueueManager &operator=(MpsCommandQueueManager &&) = default;
  ~MpsCommandQueueManager() = default;

  void initialize(
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t device,
      SlowOps *slow_ops, std::size_t capacity);

  void shutdown();

  void growCapacity(std::size_t additional);

  CommandQueueLease acquire();

  void release(CommandQueueLease &lease) noexcept;

  /// @brief Check if the command queue with the given handle is currently in
  /// use
  bool isInUse(::orteaf::internal::base::CommandQueueHandle handle) const;

  void releaseUnusedQueues();

private:
  std::size_t allocateSlot();

  void growStatePool(std::size_t additional_count);

  State &ensureActiveState(::orteaf::internal::base::CommandQueueHandle handle);

  const State &
  ensureActiveState(::orteaf::internal::base::CommandQueueHandle handle) const {
    return const_cast<MpsCommandQueueManager *>(this)->ensureActiveState(
        handle);
  }

  ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t device_{
      nullptr};
};

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

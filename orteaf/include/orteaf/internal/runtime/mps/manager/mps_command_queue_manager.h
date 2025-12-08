#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "orteaf/internal/runtime/mps/platform/wrapper/mps_command_queue.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_event.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/lease.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/diagnostics/log/log_config.h"
#include "orteaf/internal/runtime/mps/platform/mps_slow_ops.h"
#include "orteaf/internal/runtime/base/base_manager.h"

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
  ::orteaf::internal::runtime::mps::platform::wrapper::MPSCommandQueue_t command_queue{nullptr};
  bool in_use{false};
  bool on_free_list{true};
  std::uint32_t generation{0};
#if ORTEAF_MPS_DEBUG_ENABLED
  ::orteaf::internal::runtime::mps::platform::wrapper::MPSEvent_t event{nullptr};
  struct SerialState {
    std::uint64_t submit_serial{0};
    std::uint64_t completed_serial{0};
  } serial{};
  std::size_t event_refcount{0};
  std::size_t serial_refcount{0};
#endif

  void resetHazards() noexcept;

  void destroy(SlowOps *slow_ops) noexcept;
};

struct MpsCommandQueueManagerTraits {
  using DeviceType = ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t;
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
#if ORTEAF_MPS_DEBUG_ENABLED
    using EventLease = ::orteaf::internal::base::Lease<
      ::orteaf::internal::base::CommandQueueHandle,
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSEvent_t, MpsCommandQueueManager>;
  using SerialState = MpsCommandQueueManagerState::SerialState;
  using SerialLease = ::orteaf::internal::base::Lease<
      ::orteaf::internal::base::CommandQueueHandle, SerialState *,
      MpsCommandQueueManager>;
#endif

  MpsCommandQueueManager() = default;
  MpsCommandQueueManager(const MpsCommandQueueManager &) = delete;
  MpsCommandQueueManager &operator=(const MpsCommandQueueManager &) = delete;
  MpsCommandQueueManager(MpsCommandQueueManager &&) = default;
  MpsCommandQueueManager &operator=(MpsCommandQueueManager &&) = default;
  ~MpsCommandQueueManager() = default;

  void initialize(::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t device,
                  SlowOps *slow_ops, std::size_t capacity);

  void shutdown();

  void growCapacity(std::size_t additional);

  CommandQueueLease acquire();

  void release(CommandQueueLease &lease) noexcept;

#if ORTEAF_MPS_DEBUG_ENABLED
  EventLease acquireEvent(::orteaf::internal::base::CommandQueueHandle handle);
  void release(EventLease &lease) noexcept;
  SerialLease
  acquireSerial(::orteaf::internal::base::CommandQueueHandle handle);
  void release(SerialLease &lease) noexcept;
#endif

  void releaseUnusedQueues();

#if ORTEAF_ENABLE_TEST
  struct DebugState {
    std::uint32_t generation{0};
    bool in_use{false};
    bool queue_allocated{false};
    std::size_t growth_chunk_size{0};
#if ORTEAF_MPS_DEBUG_ENABLED
    std::uint64_t submit_serial{0};
    std::uint64_t completed_serial{0};
    std::size_t event_refcount{0};
    std::size_t serial_refcount{0};
#endif
  };

  DebugState
  debugState(::orteaf::internal::base::CommandQueueHandle handle) const;
#endif

private:
  std::size_t allocateSlot();

  void growStatePool(std::size_t additional_count);

  State &ensureActiveState(::orteaf::internal::base::CommandQueueHandle handle);

  const State &
  ensureActiveState(::orteaf::internal::base::CommandQueueHandle handle) const {
    return const_cast<MpsCommandQueueManager *>(this)->ensureActiveState(
        handle);
  }
};

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

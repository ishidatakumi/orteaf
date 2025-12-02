#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "orteaf/internal/backend/mps/wrapper/mps_command_queue.h"
#include "orteaf/internal/backend/mps/wrapper/mps_event.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/lease.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/diagnostics/log/log_config.h"
#include "orteaf/internal/backend/mps/mps_slow_ops.h"

namespace orteaf::internal::runtime::mps {

/**
 * @brief Stub implementation of an MPS command queue manager.
 *
 * The real implementation will manage a freelist of queues backed by
 * BackendOps. For now we only provide the API surface required by the unit
 * tests so the project builds; behaviour is intentionally minimal.
 */
class MpsCommandQueueManager {
public:
  using BackendOps = ::orteaf::internal::runtime::backend_ops::mps::MpsSlowOps;
  using CommandQueueLease = ::orteaf::internal::base::Lease<::orteaf::internal::base::CommandQueueHandle,
                                                            ::orteaf::internal::backend::mps::MPSCommandQueue_t,
                                                            MpsCommandQueueManager>;
#if ORTEAF_MPS_DEBUG_ENABLED
  using EventLease = ::orteaf::internal::base::Lease<::orteaf::internal::base::CommandQueueHandle,
                                                     ::orteaf::internal::backend::mps::MPSEvent_t,
                                                     MpsCommandQueueManager>;
  struct SerialState {
    std::uint64_t submit_serial{0};
    std::uint64_t completed_serial{0};
  };
  using SerialLease = ::orteaf::internal::base::Lease<::orteaf::internal::base::CommandQueueHandle,
                                                      SerialState*,
                                                      MpsCommandQueueManager>;
#endif

  MpsCommandQueueManager() = default;
  MpsCommandQueueManager(const MpsCommandQueueManager&) = delete;
  MpsCommandQueueManager& operator=(const MpsCommandQueueManager&) = delete;
  MpsCommandQueueManager(MpsCommandQueueManager&&) = default;
  MpsCommandQueueManager& operator=(MpsCommandQueueManager&&) = default;
  ~MpsCommandQueueManager() = default;

  void setGrowthChunkSize(std::size_t chunk) {
    if (chunk == 0) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          "Growth chunk size must be > 0");
    }
    growth_chunk_size_ = chunk;
  }

  std::size_t growthChunkSize() const noexcept { return growth_chunk_size_; }

  void initialize(::orteaf::internal::backend::mps::MPSDevice_t device,
                  BackendOps *ops, std::size_t capacity);

  void shutdown();

  std::size_t capacity() const noexcept { return states_.size(); }

  void growCapacity(std::size_t additional);

  CommandQueueLease acquire();

  void release(CommandQueueLease& lease) noexcept;

#if ORTEAF_MPS_DEBUG_ENABLED
  EventLease acquireEvent(::orteaf::internal::base::CommandQueueHandle handle);
  void release(EventLease& lease) noexcept;
  SerialLease acquireSerial(::orteaf::internal::base::CommandQueueHandle handle);
  void release(SerialLease& lease) noexcept;
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

  DebugState debugState(base::CommandQueueHandle handle) const;
#endif

private:
  struct State {
    ::orteaf::internal::backend::mps::MPSCommandQueue_t command_queue{nullptr};
    bool in_use{false};
    bool on_free_list{true};
    std::uint32_t generation{0};
#if ORTEAF_MPS_DEBUG_ENABLED
    ::orteaf::internal::backend::mps::MPSEvent_t event{nullptr};
    SerialState serial{};
    std::size_t event_refcount{0};
    std::size_t serial_refcount{0};
#endif

    void resetHazards() noexcept;

    void destroy(BackendOps *ops) noexcept;
  };

  void ensureInitialized() const;

  std::size_t allocateSlot();

  void growStatePool(std::size_t additional_count);

  State &ensureActiveState(base::CommandQueueHandle handle);

  const State &ensureActiveState(base::CommandQueueHandle handle) const {
    return const_cast<MpsCommandQueueManager *>(this)->ensureActiveState(handle);
  }

  ::orteaf::internal::base::HeapVector<State> states_;
  ::orteaf::internal::base::HeapVector<std::size_t> free_list_;
  std::size_t growth_chunk_size_{1};
  bool initialized_{false};
  ::orteaf::internal::backend::mps::MPSDevice_t device_{nullptr};
  BackendOps *ops_{nullptr};
};

} // namespace orteaf::internal::runtime::mps

#endif // ORTEAF_ENABLE_MPS

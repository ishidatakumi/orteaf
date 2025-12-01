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
#include "orteaf/internal/diagnostics/error/error.h"
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

  base::CommandQueueId acquire();

  void release(base::CommandQueueId id);

  void releaseUnusedQueues();

  ::orteaf::internal::backend::mps::MPSCommandQueue_t
  getCommandQueue(base::CommandQueueId id) const;

  std::uint64_t submitSerial(base::CommandQueueId id) const;

  void setSubmitSerial(base::CommandQueueId id, std::uint64_t value);

  std::uint64_t completedSerial(base::CommandQueueId id) const;

  void setCompletedSerial(base::CommandQueueId id, std::uint64_t value);

#if ORTEAF_ENABLE_TEST
  struct DebugState {
    std::uint64_t submit_serial{0};
    std::uint64_t completed_serial{0};
    std::uint32_t generation{0};
    bool in_use{false};
    bool queue_allocated{false};
    std::size_t growth_chunk_size{0};
  };

  DebugState debugState(base::CommandQueueId id) const;
#endif

private:
  struct State {
    ::orteaf::internal::backend::mps::MPSCommandQueue_t command_queue{nullptr};
    ::orteaf::internal::backend::mps::MPSEvent_t event{nullptr};
    std::uint64_t submit_serial{0};
    std::uint64_t completed_serial{0};
    std::uint32_t generation{0};
    bool in_use{false};

    void resetHazards() noexcept;

    void destroy(BackendOps *ops) noexcept;
  };

  void ensureInitialized() const;

  std::size_t allocateSlot();

  void growStatePool(std::size_t additional_count);

  State &ensureActiveState(base::CommandQueueId id);

  const State &ensureActiveState(base::CommandQueueId id) const {
    return const_cast<MpsCommandQueueManager *>(this)->ensureActiveState(id);
  }

  base::CommandQueueId encodeId(std::size_t index,
                                std::uint32_t generation) const;

  std::size_t indexFromId(base::CommandQueueId id) const;

  std::size_t indexFromIdRaw(base::CommandQueueId id) const;

  std::uint32_t generationFromId(base::CommandQueueId id) const;

  ::orteaf::internal::base::HeapVector<State> states_;
  ::orteaf::internal::base::HeapVector<std::size_t> free_list_;
  std::size_t growth_chunk_size_{1};
  bool initialized_{false};
  ::orteaf::internal::backend::mps::MPSDevice_t device_{nullptr};
  BackendOps *ops_{nullptr};
};

} // namespace orteaf::internal::runtime::mps

#endif // ORTEAF_ENABLE_MPS

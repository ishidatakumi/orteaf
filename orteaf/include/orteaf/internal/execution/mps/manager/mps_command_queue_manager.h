#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/lease/control_block/weak.h"
#include "orteaf/internal/base/lease/weak_lease.h"
#include "orteaf/internal/base/manager/pool_manager.h"
#include "orteaf/internal/base/pool/slot_pool.h"
#include "orteaf/internal/execution/mps/platform/mps_slow_ops.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_command_queue.h"

namespace orteaf::internal::execution::mps::manager {

// =============================================================================
// Payload Pool
// =============================================================================

struct CommandQueuePayloadPoolTraits {
  using Payload =
      ::orteaf::internal::execution::mps::platform::wrapper::MpsCommandQueue_t;
  using Handle = ::orteaf::internal::base::CommandQueueHandle;
  using DeviceType =
      ::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t;
  using SlowOps = ::orteaf::internal::execution::mps::platform::MpsSlowOps;

  struct Request {
    Handle handle{Handle::invalid()};
  };

  struct Context {
    DeviceType device{nullptr};
    SlowOps *ops{nullptr};
  };

  static bool create(Payload &payload, const Request &,
                     const Context &context) {
    if (context.ops == nullptr || context.device == nullptr) {
      return false;
    }
    auto queue = context.ops->createCommandQueue(context.device);
    if (queue == nullptr) {
      return false;
    }
    payload = queue;
    return true;
  }

  static void destroy(Payload &payload, const Request &,
                      const Context &context) {
    if (payload != nullptr && context.ops != nullptr) {
      context.ops->destroyCommandQueue(payload);
      payload = nullptr;
    }
  }
};

using CommandQueuePayloadPool =
    ::orteaf::internal::base::pool::SlotPool<CommandQueuePayloadPoolTraits>;

// =============================================================================
// ControlBlock (WeakControlBlock for non-owning references)
// =============================================================================

struct CommandQueueControlBlockTag {};

using CommandQueueControlBlock = ::orteaf::internal::base::WeakControlBlock<
    ::orteaf::internal::base::CommandQueueHandle,
    ::orteaf::internal::execution::mps::platform::wrapper::MpsCommandQueue_t,
    CommandQueuePayloadPool>;

// =============================================================================
// Manager Traits for PoolManager
// =============================================================================

struct MpsCommandQueueManagerTraits {
  using PayloadPool = CommandQueuePayloadPool;
  using ControlBlock = CommandQueueControlBlock;
  struct ControlBlockTag {};
  using PayloadHandle = ::orteaf::internal::base::CommandQueueHandle;
  static constexpr const char *Name = "MPS command queue manager";
};

// =============================================================================
// MpsCommandQueueManager
// =============================================================================

class MpsCommandQueueManager {
public:
  using SlowOps = ::orteaf::internal::execution::mps::platform::MpsSlowOps;
  using DeviceType =
      ::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t;
  using CommandQueueHandle = ::orteaf::internal::base::CommandQueueHandle;
  using CommandQueueType =
      ::orteaf::internal::execution::mps::platform::wrapper::MpsCommandQueue_t;

  using Core =
      ::orteaf::internal::base::PoolManager<MpsCommandQueueManagerTraits>;
  using ControlBlock = Core::ControlBlock;
  using ControlBlockHandle = Core::ControlBlockHandle;
  using ControlBlockPool = Core::ControlBlockPool;

  using CommandQueueLease = Core::WeakLeaseType;

private:
  friend CommandQueueLease;

public:
  struct Config {
    DeviceType device{nullptr};
    SlowOps *ops{nullptr};
    std::size_t payload_capacity{0};
    std::size_t control_block_capacity{0};
    std::size_t payload_block_size{0};
    std::size_t control_block_block_size{1};
    std::size_t payload_growth_chunk_size{1};
    std::size_t control_block_growth_chunk_size{1};
  };

  // ===========================================================================
  // Lifecycle
  // ===========================================================================

  MpsCommandQueueManager() = default;
  MpsCommandQueueManager(const MpsCommandQueueManager &) = delete;
  MpsCommandQueueManager &operator=(const MpsCommandQueueManager &) = delete;
  MpsCommandQueueManager(MpsCommandQueueManager &&) = default;
  MpsCommandQueueManager &operator=(MpsCommandQueueManager &&) = default;
  ~MpsCommandQueueManager() = default;

  void configure(const Config &config);
  void shutdown();

  // ===========================================================================
  // Acquire API
  // ===========================================================================

  /// @brief Acquire a new command queue (creates queue + control block)
  CommandQueueLease acquire();

  /// @brief Acquire a weak reference to an existing queue by handle
  /// @note Creates a new control block each time for speed
  CommandQueueLease acquire(CommandQueueHandle handle);

#if ORTEAF_ENABLE_TEST
  bool isConfiguredForTest() const noexcept { return core_.isConfigured(); }

  std::size_t payloadPoolSizeForTest() const noexcept {
    return core_.payloadPoolSizeForTest();
  }
  std::size_t payloadPoolCapacityForTest() const noexcept {
    return core_.payloadPoolCapacityForTest();
  }
  std::size_t controlBlockPoolSizeForTest() const noexcept {
    return core_.controlBlockPoolSizeForTest();
  }
  std::size_t controlBlockPoolCapacityForTest() const noexcept {
    return core_.controlBlockPoolCapacityForTest();
  }
  bool isAliveForTest(CommandQueueHandle handle) const noexcept {
    return core_.isAlive(handle);
  }
  std::size_t payloadGrowthChunkSizeForTest() const noexcept {
    return core_.payloadGrowthChunkSize();
  }
  std::size_t controlBlockGrowthChunkSizeForTest() const noexcept {
    return core_.growthChunkSize();
  }
#endif

private:
  Core core_{};
  DeviceType device_{nullptr};
  SlowOps *ops_{nullptr};
  std::size_t payload_block_size_{0};
};

} // namespace orteaf::internal::execution::mps::manager

#endif // ORTEAF_ENABLE_MPS

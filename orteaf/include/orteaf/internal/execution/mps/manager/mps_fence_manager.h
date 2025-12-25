#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/lease/control_block/strong.h"
#include "orteaf/internal/base/lease/strong_lease.h"
#include "orteaf/internal/base/manager/pool_manager.h"
#include "orteaf/internal/base/pool/slot_pool.h"
#include "orteaf/internal/execution/mps/platform/mps_slow_ops.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_fence.h"

namespace orteaf::internal::execution::mps::manager {

// =============================================================================
// Payload Pool
// =============================================================================

struct FencePayloadPoolTraits {
  using Payload =
      ::orteaf::internal::execution::mps::platform::wrapper::MpsFence_t;
  using Handle = ::orteaf::internal::base::FenceHandle;
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
    auto fence = context.ops->createFence(context.device);
    if (fence == nullptr) {
      return false;
    }
    payload = fence;
    return true;
  }

  static void destroy(Payload &payload, const Request &,
                      const Context &context) {
    if (payload != nullptr && context.ops != nullptr) {
      context.ops->destroyFence(payload);
      payload = nullptr;
    }
  }
};

using FencePayloadPool =
    ::orteaf::internal::base::pool::SlotPool<FencePayloadPoolTraits>;

// =============================================================================
// ControlBlock (using default pool traits via PoolManager)
// =============================================================================

struct FenceControlBlockTag {};

using FenceControlBlock = ::orteaf::internal::base::StrongControlBlock<
    ::orteaf::internal::base::FenceHandle,
    ::orteaf::internal::execution::mps::platform::wrapper::MpsFence_t,
    FencePayloadPool>;

// =============================================================================
// Manager Traits for PoolManager
// =============================================================================

struct MpsFenceManagerTraits {
  using PayloadPool = FencePayloadPool;
  using ControlBlock = FenceControlBlock;
  struct ControlBlockTag {};
  using PayloadHandle = ::orteaf::internal::base::FenceHandle;
  static constexpr const char *Name = "MPS fence manager";
};

// =============================================================================
// MpsFenceManager
// =============================================================================

class MpsFenceManager {
public:
  using SlowOps = ::orteaf::internal::execution::mps::platform::MpsSlowOps;
  using DeviceType =
      ::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t;
  using FenceHandle = ::orteaf::internal::base::FenceHandle;
  using FenceType =
      ::orteaf::internal::execution::mps::platform::wrapper::MpsFence_t;

  using Core = ::orteaf::internal::base::PoolManager<MpsFenceManagerTraits>;
  using ControlBlock = Core::ControlBlock;
  using ControlBlockHandle = Core::ControlBlockHandle;
  using ControlBlockPool = Core::ControlBlockPool;

  using FenceLease = Core::StrongLeaseType;

private:
  friend FenceLease;

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

  MpsFenceManager() = default;
  MpsFenceManager(const MpsFenceManager &) = delete;
  MpsFenceManager &operator=(const MpsFenceManager &) = delete;
  MpsFenceManager(MpsFenceManager &&) = default;
  MpsFenceManager &operator=(MpsFenceManager &&) = default;
  ~MpsFenceManager() = default;

  void configure(const Config &config);
  void shutdown();

  FenceLease acquire();
  void release(FenceLease &lease) noexcept { lease.release(); }

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
  bool isAliveForTest(FenceHandle handle) const noexcept {
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
  FencePayloadPoolTraits::Context makePayloadContext() const noexcept;

  DeviceType device_{nullptr};
  SlowOps *ops_{nullptr};
  Core core_{};
  std::size_t payload_block_size_{0};
};

} // namespace orteaf::internal::execution::mps::manager

#endif // ORTEAF_ENABLE_MPS

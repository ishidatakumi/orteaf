#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/execution/base/lease/control_block/strong.h"
#include "orteaf/internal/execution/base/lease/strong_lease.h"
#include "orteaf/internal/execution/base/manager/base_pool_manager_core.h"
#include "orteaf/internal/execution/base/pool/slot_pool.h"
#include "orteaf/internal/execution/mps/platform/mps_slow_ops.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_event.h"

namespace orteaf::internal::execution::mps::manager {

// =============================================================================
// Payload Pool
// =============================================================================

struct EventPayloadPoolTraits {
  using Payload =
      ::orteaf::internal::execution::mps::platform::wrapper::MpsEvent_t;
  using Handle = ::orteaf::internal::base::EventHandle;
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
    auto event = context.ops->createEvent(context.device);
    if (event == nullptr) {
      return false;
    }
    payload = event;
    return true;
  }

  static void destroy(Payload &payload, const Request &,
                      const Context &context) {
    if (payload != nullptr && context.ops != nullptr) {
      context.ops->destroyEvent(payload);
      payload = nullptr;
    }
  }
};

using EventPayloadPool =
    ::orteaf::internal::execution::base::pool::SlotPool<EventPayloadPoolTraits>;

// =============================================================================
// ControlBlock (using default pool traits via BasePoolManagerCore)
// =============================================================================

struct EventControlBlockTag {};

using EventControlBlock = ::orteaf::internal::execution::base::StrongControlBlock<
    ::orteaf::internal::base::EventHandle,
    ::orteaf::internal::execution::mps::platform::wrapper::MpsEvent_t,
    EventPayloadPool>;

// =============================================================================
// Manager Traits for BasePoolManagerCore
// =============================================================================

struct MpsEventManagerTraits {
  using PayloadPool = EventPayloadPool;
  using ControlBlock = EventControlBlock;
  struct ControlBlockTag {};
  using PayloadHandle = ::orteaf::internal::base::EventHandle;
  static constexpr const char *Name = "MPS event manager";
};

// =============================================================================
// MpsEventManager
// =============================================================================

class MpsEventManager {
public:
  using SlowOps = ::orteaf::internal::execution::mps::platform::MpsSlowOps;
  using DeviceType =
      ::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t;
  using EventHandle = ::orteaf::internal::base::EventHandle;
  using EventType =
      ::orteaf::internal::execution::mps::platform::wrapper::MpsEvent_t;

  using Core = ::orteaf::internal::execution::base::BasePoolManagerCore<
      MpsEventManagerTraits>;
  using ControlBlock = Core::ControlBlock;
  using ControlBlockHandle = Core::ControlBlockHandle;
  using ControlBlockPool = Core::ControlBlockPool;

  using EventLease = ::orteaf::internal::execution::base::StrongLease<
      ControlBlockHandle, ControlBlock, ControlBlockPool, MpsEventManager>;

private:
  friend EventLease;

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

  MpsEventManager() = default;
  MpsEventManager(const MpsEventManager &) = delete;
  MpsEventManager &operator=(const MpsEventManager &) = delete;
  MpsEventManager(MpsEventManager &&) = default;
  MpsEventManager &operator=(MpsEventManager &&) = default;
  ~MpsEventManager() = default;

  void configure(const Config &config);
  void shutdown();

  EventLease acquire();
  void release(EventLease &lease) noexcept { lease.release(); }

#if ORTEAF_ENABLE_TEST
  bool isInitializedForTest() const noexcept { return core_.isInitialized(); }

  std::size_t payloadPoolSizeForTest() const noexcept {
    return core_.payloadPool().size();
  }
  std::size_t payloadPoolCapacityForTest() const noexcept {
    return core_.payloadPool().capacity();
  }
  std::size_t controlBlockPoolSizeForTest() const noexcept {
    return core_.controlBlockPoolSizeForTest();
  }
  std::size_t controlBlockPoolCapacityForTest() const noexcept {
    return core_.controlBlockPoolCapacityForTest();
  }
  bool isAliveForTest(EventHandle handle) const noexcept {
    return core_.isAlive(handle);
  }
  std::size_t payloadGrowthChunkSizeForTest() const noexcept {
    return payload_growth_chunk_size_;
  }
  std::size_t controlBlockGrowthChunkSizeForTest() const noexcept {
    return core_.growthChunkSize();
  }
#endif

private:
  EventPayloadPoolTraits::Context makePayloadContext() const noexcept;
  EventLease buildLease(ControlBlock &cb, EventHandle payload_handle,
                        ControlBlockHandle cb_handle);

  DeviceType device_{nullptr};
  SlowOps *ops_{nullptr};
  Core core_{};
  std::size_t payload_block_size_{0};
  std::size_t payload_growth_chunk_size_{1};
};

} // namespace orteaf::internal::execution::mps::manager

#endif // ORTEAF_ENABLE_MPS

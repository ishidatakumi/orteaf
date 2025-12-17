#pragma once

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/runtime/base/lease/control_block/shared.h"
#include "orteaf/internal/runtime/base/lease/shared_lease.h"
#include "orteaf/internal/runtime/base/lease/slot.h"
#include "orteaf/internal/runtime/base/manager/base_manager_core.h"
#include "orteaf/internal/runtime/mps/platform/mps_slow_ops.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_event.h"

namespace orteaf::internal::runtime::mps::manager {

// Slot type: Standard Slot with initialization tracking
using EventSlot = ::orteaf::internal::runtime::base::GenerationalSlot<
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsEvent_t>;

// Control block: Shared ownership
using EventControlBlock =
    ::orteaf::internal::runtime::base::SharedControlBlock<EventSlot>;

struct MpsEventManagerTraits {
  using ControlBlock = EventControlBlock;
  using Handle = ::orteaf::internal::base::EventHandle;
  static constexpr const char *Name = "MpsEventManager";
};

class MpsEventManager
    : protected ::orteaf::internal::runtime::base::BaseManagerCore<
          MpsEventManagerTraits> {
  using Base =
      ::orteaf::internal::runtime::base::BaseManagerCore<MpsEventManagerTraits>;

public:
  using SlowOps = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;
  using DeviceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t;
  using EventHandle = ::orteaf::internal::base::EventHandle;
  using EventType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsEvent_t;
  using EventLease =
      ::orteaf::internal::runtime::base::SharedLease<EventHandle, EventType,
                                                     MpsEventManager>;

  MpsEventManager() = default;
  MpsEventManager(const MpsEventManager &) = delete;
  MpsEventManager &operator=(const MpsEventManager &) = delete;
  MpsEventManager(MpsEventManager &&) = default;
  MpsEventManager &operator=(MpsEventManager &&) = default;
  ~MpsEventManager() = default;

  void initialize(DeviceType device, SlowOps *ops, std::size_t capacity);
  void shutdown();

  EventLease acquire();
  EventLease acquire(EventHandle handle);
  void release(EventLease &lease) noexcept;
  void release(EventHandle handle) noexcept;

  // Expose capacity
  using Base::capacity;
  using Base::isInitialized;

#if ORTEAF_ENABLE_TEST
  using Base::controlBlockForTest;
  using Base::freeListSizeForTest;
  using Base::isInitializedForTest;
#endif

private:
  void destroyResource(EventType &resource);

  DeviceType device_{nullptr};
  SlowOps *ops_{nullptr};
};

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

#pragma once

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/runtime/base/lease/control_block/shared.h"
#include "orteaf/internal/runtime/base/lease/shared_lease.h"
#include "orteaf/internal/runtime/base/lease/slot.h"
#include "orteaf/internal/runtime/base/manager/base_manager_core.h"
#include "orteaf/internal/runtime/mps/platform/mps_slow_ops.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_fence.h"

namespace orteaf::internal::runtime::mps::manager {

// Slot type: Standard Slot with initialization tracking
using FenceSlot = ::orteaf::internal::runtime::base::GenerationalSlot<
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsFence_t>;

// Control block: Shared ownership
using FenceControlBlock =
    ::orteaf::internal::runtime::base::SharedControlBlock<FenceSlot>;

struct MpsFenceManagerTraits {
  using ControlBlock = FenceControlBlock;
  using Handle = ::orteaf::internal::base::FenceHandle;
  static constexpr const char *Name = "MPS fence manager";
};

class MpsFenceManager
    : protected ::orteaf::internal::runtime::base::BaseManagerCore<
          MpsFenceManagerTraits> {
  using Base =
      ::orteaf::internal::runtime::base::BaseManagerCore<MpsFenceManagerTraits>;

public:
  using SlowOps = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;
  using DeviceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t;
  using FenceHandle = ::orteaf::internal::base::FenceHandle;
  using FenceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsFence_t;
  using FenceLease =
      ::orteaf::internal::runtime::base::SharedLease<FenceHandle, FenceType,
                                                     MpsFenceManager>;

  MpsFenceManager() = default;
  MpsFenceManager(const MpsFenceManager &) = delete;
  MpsFenceManager &operator=(const MpsFenceManager &) = delete;
  MpsFenceManager(MpsFenceManager &&) = default;
  MpsFenceManager &operator=(MpsFenceManager &&) = default;
  ~MpsFenceManager() = default;

  void initialize(DeviceType device, SlowOps *ops, std::size_t capacity);
  void shutdown();

  FenceLease acquire();
  FenceLease acquire(FenceHandle handle);
  void release(FenceLease &lease) noexcept;
  void release(FenceHandle handle) noexcept;

  // Expose capacity
  using Base::capacity;
  using Base::isInitialized;

#if ORTEAF_ENABLE_TEST
  using Base::controlBlockForTest;
  using Base::freeListSizeForTest;
  using Base::isInitializedForTest;
#endif

private:
  void destroyResource(FenceType &resource);

  DeviceType device_{nullptr};
  SlowOps *ops_{nullptr};
};

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

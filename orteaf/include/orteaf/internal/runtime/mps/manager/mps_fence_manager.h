#pragma once

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/shared_lease.h"
#include "orteaf/internal/runtime/base/resource_manager.h"
#include "orteaf/internal/runtime/mps/platform/mps_slow_ops.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_fence.h"

#include <atomic>

namespace orteaf::internal::runtime::mps::manager {

struct FenceManagerTraits {
  using ResourceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSFence_t;
  using StateType =
      ::orteaf::internal::runtime::base::GenerationalPoolState<ResourceType>;
  using DeviceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t;
  using OpsType = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;
  using HandleType = ::orteaf::internal::base::FenceHandle;

  static constexpr const char *Name = "MPS fence manager";

  static ResourceType create(OpsType *ops, DeviceType device) {
    return ops->createFence(device);
  }

  static void destroy(OpsType *ops, ResourceType resource) {
    if (resource != nullptr) {
      ops->destroyFence(resource);
    }
  }
};

class MpsFenceManager
    : public ::orteaf::internal::runtime::base::ResourceManager<
          MpsFenceManager, FenceManagerTraits> {
public:
  using Base =
      ::orteaf::internal::runtime::base::ResourceManager<MpsFenceManager,
                                                         FenceManagerTraits>;
  using SlowOps = Base::Ops;
  using DeviceType = Base::Device;
  using FenceHandle = Base::ResourceHandle;
  using FenceLease = Base::ResourceLease;

  MpsFenceManager() = default;
  MpsFenceManager(const MpsFenceManager &) = delete;
  MpsFenceManager &operator=(const MpsFenceManager &) = delete;
  MpsFenceManager(MpsFenceManager &&) = default;
  MpsFenceManager &operator=(MpsFenceManager &&) = default;
  ~MpsFenceManager() = default;

  // Base class provides initialize, shutdown, acquire(s), release(s)
  // and debugState.
};

} // namespace orteaf::internal::runtime::mps::manager

// Extern template declaration to prevent implicit instantiation
namespace orteaf::internal::runtime::base {
extern template class ResourceManager<
    ::orteaf::internal::runtime::mps::manager::MpsFenceManager,
    ::orteaf::internal::runtime::mps::manager::FenceManagerTraits>;
}

#endif // ORTEAF_ENABLE_MPS

#pragma once

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/runtime/base/resource_pool.h"
#include "orteaf/internal/runtime/mps/platform/mps_slow_ops.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_fence.h"

namespace orteaf::internal::runtime::mps::manager {

struct FencePoolTraits {
  using ResourceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSFence_t;
  using DeviceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t;
  using OpsType = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;

  static constexpr const char *Name = "MPS fence pool";

  static ResourceType create(OpsType *ops, DeviceType device) {
    return ops->createFence(device);
  }
  static void destroy(OpsType *ops, ResourceType res) {
    ops->destroyFence(res);
  }
};

class MpsFencePool
    : public ::orteaf::internal::runtime::base::ResourcePool<MpsFencePool,
                                                             FencePoolTraits> {
public:
  using Base = ::orteaf::internal::runtime::base::ResourcePool<MpsFencePool,
                                                               FencePoolTraits>;
  using SlowOps = Base::Ops;
  using Fence = Base::Resource;
  using FenceLease = Base::PoolLease;

  MpsFencePool() = default;
  MpsFencePool(const MpsFencePool &) = delete;
  MpsFencePool &operator=(const MpsFencePool &) = delete;
  MpsFencePool(MpsFencePool &&) = default;
  MpsFencePool &operator=(MpsFencePool &&) = default;
  ~MpsFencePool() = default;

  FenceLease acquireFence() { return acquire(); }
};

} // namespace orteaf::internal::runtime::mps::manager

namespace orteaf::internal::runtime::base {
extern template class ResourcePool<
    ::orteaf::internal::runtime::mps::manager::MpsFencePool,
    ::orteaf::internal::runtime::mps::manager::FencePoolTraits>;
}

#endif // ORTEAF_ENABLE_MPS

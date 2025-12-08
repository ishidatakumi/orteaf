#pragma once

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/runtime/base/resource_pool.h"
#include "orteaf/internal/runtime/mps/platform/mps_slow_ops.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_event.h"

namespace orteaf::internal::runtime::mps::manager {

struct EventPoolTraits {
  using ResourceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSEvent_t;
  using DeviceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t;
  using OpsType = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;

  static constexpr const char *Name = "MPS event pool";

  static ResourceType create(OpsType *ops, DeviceType device) {
    return ops->createEvent(device);
  }
  static void destroy(OpsType *ops, ResourceType res) {
    ops->destroyEvent(res);
  }
};

class MpsEventPool
    : public ::orteaf::internal::runtime::base::ResourcePool<MpsEventPool,
                                                             EventPoolTraits> {
public:
  using Base = ::orteaf::internal::runtime::base::ResourcePool<MpsEventPool,
                                                               EventPoolTraits>;
  using SlowOps = Base::Ops;
  using Event = Base::Resource;
  using EventLease = Base::PoolLease;

  MpsEventPool() = default;
  MpsEventPool(const MpsEventPool &) = delete;
  MpsEventPool &operator=(const MpsEventPool &) = delete;
  MpsEventPool(MpsEventPool &&) = default;
  MpsEventPool &operator=(MpsEventPool &&) = default;
  ~MpsEventPool() = default;

  EventLease acquireEvent() { return acquire(); }
};

} // namespace orteaf::internal::runtime::mps::manager

namespace orteaf::internal::runtime::base {
extern template class ResourcePool<
    ::orteaf::internal::runtime::mps::manager::MpsEventPool,
    ::orteaf::internal::runtime::mps::manager::EventPoolTraits>;
}

#endif // ORTEAF_ENABLE_MPS

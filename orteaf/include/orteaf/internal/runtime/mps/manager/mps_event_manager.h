#pragma once

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/shared_lease.h"
#include "orteaf/internal/runtime/base/resource_manager.h"
#include "orteaf/internal/runtime/mps/platform/mps_slow_ops.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_event.h"

#include <atomic>

namespace orteaf::internal::runtime::mps::manager {

struct EventManagerTraits {
  using ResourceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSEvent_t;
  using StateType =
      ::orteaf::internal::runtime::base::GenerationalPoolState<ResourceType>;
  using DeviceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t;
  using OpsType = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;
  using HandleType = ::orteaf::internal::base::EventHandle;

  static constexpr const char *Name = "MPS event manager";

  static ResourceType create(OpsType *ops, DeviceType device) {
    return ops->createEvent(device);
  }

  static void destroy(OpsType *ops, ResourceType resource) {
    if (resource != nullptr) {
      ops->destroyEvent(resource);
    }
  }
};

class MpsEventManager
    : public ::orteaf::internal::runtime::base::ResourceManager<
          MpsEventManager, EventManagerTraits> {
public:
  using Base =
      ::orteaf::internal::runtime::base::ResourceManager<MpsEventManager,
                                                         EventManagerTraits>;
  using SlowOps = Base::Ops;
  using DeviceType = Base::Device;
  using EventHandle = Base::ResourceHandle;
  using EventLease = Base::ResourceLease;

  MpsEventManager() = default;
  MpsEventManager(const MpsEventManager &) = delete;
  MpsEventManager &operator=(const MpsEventManager &) = delete;
  MpsEventManager(MpsEventManager &&) = default;
  MpsEventManager &operator=(MpsEventManager &&) = default;
  ~MpsEventManager() = default;

  // Base class provides initialize, shutdown, acquire(s), release(s)
  // and debugState.
};

} // namespace orteaf::internal::runtime::mps::manager

// Extern template declaration to prevent implicit instantiation
namespace orteaf::internal::runtime::base {
extern template class ResourceManager<
    ::orteaf::internal::runtime::mps::manager::MpsEventManager,
    ::orteaf::internal::runtime::mps::manager::EventManagerTraits>;
}

#endif // ORTEAF_ENABLE_MPS

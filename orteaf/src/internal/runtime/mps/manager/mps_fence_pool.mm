#include "orteaf/internal/runtime/mps/manager/mps_fence_pool.h"

#if ORTEAF_ENABLE_MPS

namespace orteaf::internal::runtime::mps::manager {
} // namespace orteaf::internal::runtime::mps::manager

namespace orteaf::internal::runtime::base {
template class ResourcePool<::orteaf::internal::runtime::mps::manager::MpsFencePool, ::orteaf::internal::runtime::mps::manager::FencePoolTraits>;
}

#endif // ORTEAF_ENABLE_MPS

#include "orteaf/internal/runtime/manager/mps/mps_fence_pool.h"

#if ORTEAF_ENABLE_MPS

namespace orteaf::internal::runtime::mps {
} // namespace orteaf::internal::runtime::mps

namespace orteaf::internal::runtime::base {
template class ResourcePool<::orteaf::internal::runtime::mps::MpsFencePool, ::orteaf::internal::runtime::mps::FencePoolTraits>;
}

#endif // ORTEAF_ENABLE_MPS

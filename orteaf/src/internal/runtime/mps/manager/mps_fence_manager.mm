#include "orteaf/internal/runtime/mps/manager/mps_fence_manager.h"

#if ORTEAF_ENABLE_MPS

namespace orteaf::internal::runtime::mps::manager {

// MpsFenceManager implementation is now generic in ResourceManager

} // namespace orteaf::internal::runtime::mps::manager

// Explicit template instantiation to reduce compilation time
namespace orteaf::internal::runtime::base {
template class ResourceManager<
    ::orteaf::internal::runtime::mps::manager::MpsFenceManager,
    ::orteaf::internal::runtime::mps::manager::FenceManagerTraits>;
}

#endif // ORTEAF_ENABLE_MPS

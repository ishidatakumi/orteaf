#include "orteaf/internal/runtime/mps/manager/mps_event_manager.h"

#if ORTEAF_ENABLE_MPS

namespace orteaf::internal::runtime::mps::manager {

// MpsEventManager implementation is now generic in ResourceManager

} // namespace orteaf::internal::runtime::mps::manager

// Explicit template instantiation to reduce compilation time
namespace orteaf::internal::runtime::base {
template class ResourceManager<
    ::orteaf::internal::runtime::mps::manager::MpsEventManager,
    ::orteaf::internal::runtime::mps::manager::EventManagerTraits>;
}

#endif // ORTEAF_ENABLE_MPS

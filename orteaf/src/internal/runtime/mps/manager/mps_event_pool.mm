#include "orteaf/internal/runtime/mps/manager/mps_event_pool.h"

#if ORTEAF_ENABLE_MPS

namespace orteaf::internal::runtime::mps::manager {
} // namespace orteaf::internal::runtime::mps::manager

namespace orteaf::internal::runtime::base {
template class ResourcePool<::orteaf::internal::runtime::mps::manager::MpsEventPool, ::orteaf::internal::runtime::mps::manager::EventPoolTraits>;
}

#endif // ORTEAF_ENABLE_MPS

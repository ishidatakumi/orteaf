#include "orteaf/internal/runtime/manager/mps/mps_event_pool.h"

#if ORTEAF_ENABLE_MPS

namespace orteaf::internal::runtime::mps {
} // namespace orteaf::internal::runtime::mps

namespace orteaf::internal::runtime::base {
template class ResourcePool<::orteaf::internal::runtime::mps::MpsEventPool, ::orteaf::internal::runtime::mps::EventPoolTraits>;
}

#endif // ORTEAF_ENABLE_MPS

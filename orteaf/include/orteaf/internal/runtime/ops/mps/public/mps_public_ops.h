#pragma once

#if ORTEAF_ENABLE_MPS

#include <memory>

#include "orteaf/internal/backend/mps/mps_slow_ops.h"
#include "orteaf/internal/runtime/manager/mps/mps_runtime_manager.h"

namespace orteaf::internal::runtime::ops::mps {

class MpsPublicOps {
public:
    MpsPublicOps() = default;
    MpsPublicOps(const MpsPublicOps&) = default;
    MpsPublicOps& operator=(const MpsPublicOps&) = default;
    MpsPublicOps(MpsPublicOps&&) = default;
    MpsPublicOps& operator=(MpsPublicOps&&) = default;
    ~MpsPublicOps() = default;

    using SlowOps = ::orteaf::internal::runtime::backend_ops::mps::MpsSlowOps;

    // Initialize runtime with a provided SlowOps implementation, or default MpsSlowOpsImpl.
    void initialize(std::unique_ptr<SlowOps> slow_ops = nullptr) {
        ::orteaf::internal::runtime::mps::GetMpsRuntimeManager().initialize(std::move(slow_ops));
    }

    void shutdown() {
        ::orteaf::internal::runtime::mps::GetMpsRuntimeManager().shutdown();
    }
};

}  // namespace orteaf::internal::runtime::ops::mps

#endif  // ORTEAF_ENABLE_MPS

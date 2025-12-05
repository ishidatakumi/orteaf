#pragma once

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/runtime/manager/mps/mps_runtime_manager.h"

namespace orteaf::internal::runtime::ops::mps {

class MpsPublicOps;
class MpsPrivateOps;

class MpsCommonOps {
    friend class MpsPublicOps;
    friend class MpsPrivateOps;

public:
    MpsCommonOps() = default;
    MpsCommonOps(const MpsCommonOps&) = default;
    MpsCommonOps& operator=(const MpsCommonOps&) = default;
    MpsCommonOps(MpsCommonOps&&) = default;
    MpsCommonOps& operator=(MpsCommonOps&&) = default;
    ~MpsCommonOps() = default;

private:
    static ::orteaf::internal::runtime::mps::MpsRuntimeManager& runtime() {
        static ::orteaf::internal::runtime::mps::MpsRuntimeManager instance{};
        return instance;
    }
};

}  // namespace orteaf::internal::runtime::ops::mps

#endif  // ORTEAF_ENABLE_MPS


#pragma once

#if ORTEAF_ENABLE_MPS

#include <orteaf/internal/runtime/mps/manager/mps_runtime_manager.h>

namespace orteaf::internal::runtime::mps::ops {
class MpsPublicOps;
}
namespace orteaf::internal::runtime::mps::ops {
class MpsPrivateOps;
}

namespace orteaf::internal::runtime::mps::ops {

class MpsCommonOps {
  friend class MpsPublicOps;
  friend class MpsPrivateOps;

public:
  MpsCommonOps() = default;
  MpsCommonOps(const MpsCommonOps &) = default;
  MpsCommonOps &operator=(const MpsCommonOps &) = default;
  MpsCommonOps(MpsCommonOps &&) = default;
  MpsCommonOps &operator=(MpsCommonOps &&) = default;
  ~MpsCommonOps() = default;

  static ::orteaf::internal::runtime::mps::manager::MpsRuntimeManager &
  runtime() {
    static ::orteaf::internal::runtime::mps::manager::MpsRuntimeManager
        instance{};
    return instance;
  }
};

} // namespace orteaf::internal::runtime::mps::ops

#endif // ORTEAF_ENABLE_MPS

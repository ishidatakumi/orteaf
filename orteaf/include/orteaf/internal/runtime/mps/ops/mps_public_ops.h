#pragma once

#if ORTEAF_ENABLE_MPS

#include <memory>

#include "orteaf/internal/runtime/mps/ops/mps_common_ops.h"
#include "orteaf/internal/runtime/mps/platform/mps_slow_ops.h"

namespace orteaf::internal::runtime::mps::ops {

class MpsPublicOps {
public:
  MpsPublicOps() = default;
  MpsPublicOps(const MpsPublicOps &) = default;
  MpsPublicOps &operator=(const MpsPublicOps &) = default;
  MpsPublicOps(MpsPublicOps &&) = default;
  MpsPublicOps &operator=(MpsPublicOps &&) = default;
  ~MpsPublicOps() = default;

  using SlowOps = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;

  // Initialize runtime with a provided SlowOps implementation, or default
  // MpsSlowOpsImpl.
  void initialize(std::unique_ptr<SlowOps> slow_ops = nullptr) {
    MpsCommonOps::runtime().initialize(std::move(slow_ops));
  }

  void shutdown() { MpsCommonOps::runtime().shutdown(); }
};

} // namespace orteaf::internal::runtime::mps::ops

#endif // ORTEAF_ENABLE_MPS

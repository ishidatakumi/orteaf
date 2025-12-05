#pragma once

#if ORTEAF_ENABLE_MPS

#include <memory>

#include "orteaf/internal/backend/mps/mps_slow_ops.h"
#include "orteaf/internal/runtime/manager/mps/mps_device_manager.h"

namespace orteaf::internal::runtime::mps {

class MpsRuntimeManager {
public:
    MpsRuntimeManager() = default;
    MpsRuntimeManager(const MpsRuntimeManager&) = delete;
    MpsRuntimeManager& operator=(const MpsRuntimeManager&) = delete;
    MpsRuntimeManager(MpsRuntimeManager&&) = default;
    MpsRuntimeManager& operator=(MpsRuntimeManager&&) = default;
    ~MpsRuntimeManager() = default;

    MpsDeviceManager& deviceManager() noexcept { return device_manager_; }
    const MpsDeviceManager& deviceManager() const noexcept { return device_manager_; }

    void initialize(std::unique_ptr<SlowOps> slow_ops = nullptr) {
        if (slow_ops) {
            slow_ops_ = std::move(slow_ops);
        } else if (!slow_ops_) {
            slow_ops_ = std::make_unique<SlowOpsImpl>();
        }
        device_manager_.initialize(slow_ops_.get());
    }

    void shutdown() {
        device_manager_.shutdown();
        slow_ops_.reset();
    }

private:
    using SlowOps = ::orteaf::internal::runtime::backend_ops::mps::MpsSlowOps;
    using SlowOpsImpl = ::orteaf::internal::runtime::backend_ops::mps::MpsSlowOpsImpl;

    MpsDeviceManager device_manager_{};
    std::unique_ptr<SlowOps> slow_ops_{};
};

}  // namespace orteaf::internal::runtime::mps

#endif  // ORTEAF_ENABLE_MPS

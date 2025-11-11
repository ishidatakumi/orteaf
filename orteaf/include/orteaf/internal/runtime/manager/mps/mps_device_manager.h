#pragma once

#include "orteaf/internal/architecture/architecture.h"
#include "orteaf/internal/architecture/mps_detect.h"
#include "orteaf/internal/backend/mps/mps_device.h"
#include "orteaf/internal/base/strong_id.h"
#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::runtime::mps {

class MpsDeviceManager {
public:
    void initializeDevices() {
        state_.is_alive = true;
    }

    void shutdown() {
        state_.is_alive = false;
    }

    std::size_t getDeviceCount() const {
        return state_.is_alive ? 1u : 0u;
    }

    ::orteaf::internal::architecture::Architecture getArch(::orteaf::internal::base::DeviceId) const {
        return state_.arch;
    }

    bool isAlive(::orteaf::internal::base::DeviceId) const {
        return state_.is_alive;
    }

private:
    struct State {
        ::orteaf::internal::architecture::Architecture arch{
            ::orteaf::internal::architecture::Architecture::mps_generic};
        bool is_alive{false};
    };
    State state_;
};

inline MpsDeviceManager MpsDeviceManager{};

} // namespace orteaf::internal::runtime::mps

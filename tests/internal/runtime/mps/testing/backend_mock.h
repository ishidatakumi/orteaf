#pragma once

#include <gmock/gmock.h>

#include "tests/internal/testing/static_mock.h"
#include "orteaf/internal/runtime/backend_ops/mps/mps_backend_ops.h"

namespace orteaf::tests::runtime::mps {

struct MpsBackendOpsMock {
    MOCK_METHOD(int, getDeviceCount, ());
    MOCK_METHOD(::orteaf::internal::backend::mps::MPSDevice_t, getDevice,
                (::orteaf::internal::backend::mps::MPSInt_t));
    MOCK_METHOD(void, releaseDevice, (::orteaf::internal::backend::mps::MPSDevice_t));
    MOCK_METHOD(::orteaf::internal::architecture::Architecture, detectArchitecture,
                (::orteaf::internal::base::DeviceId));
};

using MpsBackendOpsMockRegistry = ::orteaf::tests::StaticMockRegistry<MpsBackendOpsMock>;

struct MpsBackendOpsMockAdapter {
    static int getDeviceCount() {
        return MpsBackendOpsMockRegistry::get().getDeviceCount();
    }

    static ::orteaf::internal::backend::mps::MPSDevice_t getDevice(::orteaf::internal::backend::mps::MPSInt_t index) {
        return MpsBackendOpsMockRegistry::get().getDevice(index);
    }

    static void releaseDevice(::orteaf::internal::backend::mps::MPSDevice_t device) {
        MpsBackendOpsMockRegistry::get().releaseDevice(device);
    }

    static ::orteaf::internal::architecture::Architecture detectArchitecture(::orteaf::internal::base::DeviceId id) {
        return MpsBackendOpsMockRegistry::get().detectArchitecture(id);
    }
};

}  // namespace orteaf::tests::runtime::mps

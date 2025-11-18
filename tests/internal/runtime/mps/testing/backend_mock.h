#pragma once

#include <string_view>

#include <gmock/gmock.h>

#include "tests/internal/testing/static_mock.h"
#include "orteaf/internal/runtime/backend_ops/mps/mps_backend_ops.h"
#include "orteaf/internal/backend/mps/mps_command_queue.h"
#include "orteaf/internal/backend/mps/mps_event.h"
#include "orteaf/internal/backend/mps/mps_library.h"
#include "orteaf/internal/base/strong_id.h"

namespace orteaf::tests::runtime::mps {

struct MpsBackendOpsMock {
    MOCK_METHOD(int, getDeviceCount, ());
    MOCK_METHOD(::orteaf::internal::backend::mps::MPSDevice_t, getDevice,
                (::orteaf::internal::backend::mps::MPSInt_t));
    MOCK_METHOD(void, releaseDevice, (::orteaf::internal::backend::mps::MPSDevice_t));
    MOCK_METHOD(::orteaf::internal::architecture::Architecture, detectArchitecture,
                (::orteaf::internal::base::DeviceId));
    MOCK_METHOD(::orteaf::internal::backend::mps::MPSCommandQueue_t, createCommandQueue,
                (::orteaf::internal::backend::mps::MPSDevice_t));
    MOCK_METHOD(void, destroyCommandQueue,
                (::orteaf::internal::backend::mps::MPSCommandQueue_t));
    MOCK_METHOD(::orteaf::internal::backend::mps::MPSEvent_t, createEvent,
                (::orteaf::internal::backend::mps::MPSDevice_t));
    MOCK_METHOD(void, destroyEvent,
                (::orteaf::internal::backend::mps::MPSEvent_t));
    MOCK_METHOD(::orteaf::internal::backend::mps::MPSLibrary_t, createLibraryWithName,
                (::orteaf::internal::backend::mps::MPSDevice_t, std::string_view));
    MOCK_METHOD(void, destroyLibrary,
                (::orteaf::internal::backend::mps::MPSLibrary_t));
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

    static ::orteaf::internal::backend::mps::MPSCommandQueue_t createCommandQueue(
            ::orteaf::internal::backend::mps::MPSDevice_t device) {
        return MpsBackendOpsMockRegistry::get().createCommandQueue(device);
    }

    static void destroyCommandQueue(::orteaf::internal::backend::mps::MPSCommandQueue_t command_queue) {
        MpsBackendOpsMockRegistry::get().destroyCommandQueue(command_queue);
    }

    static ::orteaf::internal::backend::mps::MPSEvent_t createEvent(
            ::orteaf::internal::backend::mps::MPSDevice_t device) {
        return MpsBackendOpsMockRegistry::get().createEvent(device);
    }

    static void destroyEvent(::orteaf::internal::backend::mps::MPSEvent_t event) {
        MpsBackendOpsMockRegistry::get().destroyEvent(event);
    }

    static ::orteaf::internal::backend::mps::MPSLibrary_t createLibraryWithName(
            ::orteaf::internal::backend::mps::MPSDevice_t device,
            std::string_view name) {
        return MpsBackendOpsMockRegistry::get().createLibraryWithName(device, name);
    }

    static void destroyLibrary(::orteaf::internal::backend::mps::MPSLibrary_t library) {
        MpsBackendOpsMockRegistry::get().destroyLibrary(library);
    }
};

}  // namespace orteaf::tests::runtime::mps

#pragma once

#include <string_view>

#include "orteaf/internal/architecture/architecture.h"
#include "orteaf/internal/architecture/mps_detect.h"
#include "orteaf/internal/backend/mps/mps_command_queue.h"
#include "orteaf/internal/backend/mps/mps_device.h"
#include "orteaf/internal/backend/mps/mps_event.h"
#include "orteaf/internal/backend/mps/mps_library.h"
#include "orteaf/internal/backend/mps/mps_string.h"
#include "orteaf/internal/base/strong_id.h"

namespace orteaf::internal::runtime::backend_ops::mps {

struct MpsBackendOps {
    static int getDeviceCount() {
        return ::orteaf::internal::backend::mps::getDeviceCount();
    }

    static ::orteaf::internal::backend::mps::MPSDevice_t getDevice(::orteaf::internal::backend::mps::MPSInt_t index) {
        return ::orteaf::internal::backend::mps::getDevice(index);
    }

    static void releaseDevice(::orteaf::internal::backend::mps::MPSDevice_t device) {
        ::orteaf::internal::backend::mps::deviceRelease(device);
    }

    static ::orteaf::internal::architecture::Architecture detectArchitecture(::orteaf::internal::base::DeviceId device_id) {
        return ::orteaf::internal::architecture::detectMpsArchitectureForDeviceId(device_id);
    }

    static ::orteaf::internal::backend::mps::MPSCommandQueue_t createCommandQueue(::orteaf::internal::backend::mps::MPSDevice_t device) {
        return ::orteaf::internal::backend::mps::createCommandQueue(device);
    }

    static void destroyCommandQueue(::orteaf::internal::backend::mps::MPSCommandQueue_t queue) {
        ::orteaf::internal::backend::mps::destroyCommandQueue(queue);
    }

    static ::orteaf::internal::backend::mps::MPSEvent_t createEvent(::orteaf::internal::backend::mps::MPSDevice_t device) {
        return ::orteaf::internal::backend::mps::createEvent(device);
    }

    static void destroyEvent(::orteaf::internal::backend::mps::MPSEvent_t event) {
        ::orteaf::internal::backend::mps::destroyEvent(event);
    }

    static ::orteaf::internal::backend::mps::MPSLibrary_t createLibraryWithName(
        ::orteaf::internal::backend::mps::MPSDevice_t device,
        std::string_view name) {
        const auto ns_name = ::orteaf::internal::backend::mps::toNsString(name);
        return ::orteaf::internal::backend::mps::createLibrary(device, ns_name);
    }

    static void destroyLibrary(::orteaf::internal::backend::mps::MPSLibrary_t library) {
        ::orteaf::internal::backend::mps::destroyLibrary(library);
    }
};

}  // namespace orteaf::internal::runtime::backend_ops::mps

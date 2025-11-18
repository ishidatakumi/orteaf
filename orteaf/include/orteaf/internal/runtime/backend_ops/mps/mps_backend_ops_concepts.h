#pragma once

#include <concepts>
#include <string_view>

#include "orteaf/internal/architecture/architecture.h"
#include "orteaf/internal/backend/mps/mps_command_queue.h"
#include "orteaf/internal/backend/mps/mps_device.h"
#include "orteaf/internal/backend/mps/mps_event.h"
#include "orteaf/internal/backend/mps/mps_library.h"
#include "orteaf/internal/base/strong_id.h"

namespace orteaf::internal::runtime::backend_ops::mps {

/**
 * Concept describing the operations required from an MPS backend ops provider.
 *
 * The same provider is used by device and command queue managers, so the concept
 * aggregates all required entry points.
 */
template <class BackendOps>
concept MpsRuntimeBackendOps = requires(
    ::orteaf::internal::backend::mps::MPSDevice_t device,
    ::orteaf::internal::backend::mps::MPSInt_t device_index,
    ::orteaf::internal::backend::mps::MPSCommandQueue_t queue,
    ::orteaf::internal::backend::mps::MPSEvent_t event,
    ::orteaf::internal::backend::mps::MPSLibrary_t library,
    std::string_view library_name,
    ::orteaf::internal::base::DeviceId device_id) {
    { BackendOps::getDeviceCount() } -> std::same_as<int>;
    { BackendOps::getDevice(device_index) }
        -> std::same_as<::orteaf::internal::backend::mps::MPSDevice_t>;
    { BackendOps::releaseDevice(device) } -> std::same_as<void>;
    { BackendOps::detectArchitecture(device_id) }
        -> std::same_as<::orteaf::internal::architecture::Architecture>;

    { BackendOps::createCommandQueue(device) }
        -> std::same_as<::orteaf::internal::backend::mps::MPSCommandQueue_t>;
    { BackendOps::destroyCommandQueue(queue) } -> std::same_as<void>;
    { BackendOps::createEvent(device) }
        -> std::same_as<::orteaf::internal::backend::mps::MPSEvent_t>;
    { BackendOps::destroyEvent(event) } -> std::same_as<void>;
    { BackendOps::createLibraryWithName(device, library_name) }
        -> std::same_as<::orteaf::internal::backend::mps::MPSLibrary_t>;
    { BackendOps::destroyLibrary(library) } -> std::same_as<void>;
};

}  // namespace orteaf::internal::runtime::backend_ops::mps

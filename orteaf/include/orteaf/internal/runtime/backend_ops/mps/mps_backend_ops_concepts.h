#pragma once

#include <concepts>
#include <string_view>

#include "orteaf/internal/architecture/architecture.h"
#include "orteaf/internal/backend/mps/mps_command_queue.h"
#include "orteaf/internal/backend/mps/mps_compute_pipeline_state.h"
#include "orteaf/internal/backend/mps/mps_device.h"
#include "orteaf/internal/backend/mps/mps_event.h"
#include "orteaf/internal/backend/mps/mps_function.h"
#include "orteaf/internal/backend/mps/mps_heap.h"
#include "orteaf/internal/backend/mps/mps_library.h"
#include "orteaf/internal/backend/mps/mps_fence.h"
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
    ::orteaf::internal::backend::mps::MPSFunction_t function,
    ::orteaf::internal::backend::mps::MPSComputePipelineState_t pipeline_state,
    ::orteaf::internal::backend::mps::MPSHeapDescriptor_t heap_descriptor,
    ::orteaf::internal::backend::mps::MPSHeap_t heap,
    ::orteaf::internal::backend::mps::MPSFence_t fence,
    std::string_view library_name,
    std::string_view function_name,
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
    { BackendOps::createFence(device) }
        -> std::same_as<::orteaf::internal::backend::mps::MPSFence_t>;
    { BackendOps::destroyFence(fence) } -> std::same_as<void>;
    { BackendOps::createLibraryWithName(device, library_name) }
        -> std::same_as<::orteaf::internal::backend::mps::MPSLibrary_t>;
    { BackendOps::destroyLibrary(library) } -> std::same_as<void>;
    { BackendOps::createFunction(library, function_name) }
        -> std::same_as<::orteaf::internal::backend::mps::MPSFunction_t>;
    { BackendOps::destroyFunction(function) } -> std::same_as<void>;
    { BackendOps::createComputePipelineState(device, function) }
        -> std::same_as<::orteaf::internal::backend::mps::MPSComputePipelineState_t>;
    { BackendOps::destroyComputePipelineState(pipeline_state) } -> std::same_as<void>;
    { BackendOps::createHeapDescriptor() }
        -> std::same_as<::orteaf::internal::backend::mps::MPSHeapDescriptor_t>;
    { BackendOps::destroyHeapDescriptor(heap_descriptor) } -> std::same_as<void>;
    { BackendOps::setHeapDescriptorSize(heap_descriptor, std::size_t{0}) } -> std::same_as<void>;
    { BackendOps::setHeapDescriptorResourceOptions(heap_descriptor,
        ::orteaf::internal::backend::mps::MPSResourceOptions_t{0}) } -> std::same_as<void>;
    { BackendOps::setHeapDescriptorStorageMode(heap_descriptor,
        ::orteaf::internal::backend::mps::MPSStorageMode_t{0}) } -> std::same_as<void>;
    { BackendOps::setHeapDescriptorCPUCacheMode(heap_descriptor,
        ::orteaf::internal::backend::mps::MPSCPUCacheMode_t{0}) } -> std::same_as<void>;
    { BackendOps::setHeapDescriptorHazardTrackingMode(heap_descriptor,
        ::orteaf::internal::backend::mps::MPSHazardTrackingMode_t{0}) } -> std::same_as<void>;
    { BackendOps::setHeapDescriptorType(heap_descriptor,
        ::orteaf::internal::backend::mps::MPSHeapType_t{0}) } -> std::same_as<void>;
    { BackendOps::createHeap(device, heap_descriptor) }
        -> std::same_as<::orteaf::internal::backend::mps::MPSHeap_t>;
    { BackendOps::destroyHeap(heap) } -> std::same_as<void>;
};

}  // namespace orteaf::internal::runtime::backend_ops::mps

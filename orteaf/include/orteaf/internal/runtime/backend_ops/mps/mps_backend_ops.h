#pragma once

#include <string_view>

#include "orteaf/internal/architecture/architecture.h"
#include "orteaf/internal/architecture/mps_detect.h"
#include "orteaf/internal/backend/mps/mps_command_queue.h"
#include "orteaf/internal/backend/mps/mps_compute_pipeline_state.h"
#include "orteaf/internal/backend/mps/mps_device.h"
#include "orteaf/internal/backend/mps/mps_event.h"
#include "orteaf/internal/backend/mps/mps_function.h"
#include "orteaf/internal/backend/mps/mps_heap.h"
#include "orteaf/internal/backend/mps/mps_library.h"
#include "orteaf/internal/backend/mps/mps_fence.h"
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

    static ::orteaf::internal::backend::mps::MPSFence_t createFence(::orteaf::internal::backend::mps::MPSDevice_t device) {
        return ::orteaf::internal::backend::mps::createFence(device);
    }

    static void destroyFence(::orteaf::internal::backend::mps::MPSFence_t fence) {
        ::orteaf::internal::backend::mps::destroyFence(fence);
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

    static ::orteaf::internal::backend::mps::MPSFunction_t createFunction(
        ::orteaf::internal::backend::mps::MPSLibrary_t library,
        std::string_view name) {
        return ::orteaf::internal::backend::mps::createFunction(library, name);
    }

    static void destroyFunction(::orteaf::internal::backend::mps::MPSFunction_t function) {
        ::orteaf::internal::backend::mps::destroyFunction(function);
    }

    static ::orteaf::internal::backend::mps::MPSComputePipelineState_t createComputePipelineState(
        ::orteaf::internal::backend::mps::MPSDevice_t device,
        ::orteaf::internal::backend::mps::MPSFunction_t function) {
        return ::orteaf::internal::backend::mps::createComputePipelineState(device, function);
    }

    static void destroyComputePipelineState(
        ::orteaf::internal::backend::mps::MPSComputePipelineState_t pipeline_state) {
        ::orteaf::internal::backend::mps::destroyComputePipelineState(pipeline_state);
    }

    static ::orteaf::internal::backend::mps::MPSHeapDescriptor_t createHeapDescriptor() {
        return ::orteaf::internal::backend::mps::createHeapDescriptor();
    }

    static void destroyHeapDescriptor(::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor) {
        ::orteaf::internal::backend::mps::destroyHeapDescriptor(descriptor);
    }

    static void setHeapDescriptorSize(
        ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
        std::size_t size) {
        ::orteaf::internal::backend::mps::setHeapDescriptorSize(descriptor, size);
    }

    static void setHeapDescriptorResourceOptions(
        ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
        ::orteaf::internal::backend::mps::MPSResourceOptions_t options) {
        ::orteaf::internal::backend::mps::setHeapDescriptorResourceOptions(descriptor, options);
    }

    static void setHeapDescriptorStorageMode(
        ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
        ::orteaf::internal::backend::mps::MPSStorageMode_t storage_mode) {
        ::orteaf::internal::backend::mps::setHeapDescriptorStorageMode(descriptor, storage_mode);
    }

    static void setHeapDescriptorCPUCacheMode(
        ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
        ::orteaf::internal::backend::mps::MPSCPUCacheMode_t cpu_cache_mode) {
        ::orteaf::internal::backend::mps::setHeapDescriptorCPUCacheMode(descriptor, cpu_cache_mode);
    }

    static void setHeapDescriptorHazardTrackingMode(
        ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
        ::orteaf::internal::backend::mps::MPSHazardTrackingMode_t hazard_mode) {
        ::orteaf::internal::backend::mps::setHeapDescriptorHazardTrackingMode(descriptor, hazard_mode);
    }

    static void setHeapDescriptorType(
        ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
        ::orteaf::internal::backend::mps::MPSHeapType_t type) {
        ::orteaf::internal::backend::mps::setHeapDescriptorType(descriptor, type);
    }

    static ::orteaf::internal::backend::mps::MPSHeap_t createHeap(
        ::orteaf::internal::backend::mps::MPSDevice_t device,
        ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor) {
        return ::orteaf::internal::backend::mps::createHeap(device, descriptor);
    }

    static void destroyHeap(::orteaf::internal::backend::mps::MPSHeap_t heap) {
        ::orteaf::internal::backend::mps::destroyHeap(heap);
    }
};

}  // namespace orteaf::internal::runtime::backend_ops::mps

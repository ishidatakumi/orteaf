#pragma once

#include <string_view>

#include <gmock/gmock.h>

#include "tests/internal/testing/static_mock.h"
#include "orteaf/internal/runtime/backend_ops/mps/mps_backend_ops.h"
#include "orteaf/internal/backend/mps/mps_command_queue.h"
#include "orteaf/internal/backend/mps/mps_compute_pipeline_state.h"
#include "orteaf/internal/backend/mps/mps_event.h"
#include "orteaf/internal/backend/mps/mps_function.h"
#include "orteaf/internal/backend/mps/mps_heap.h"
#include "orteaf/internal/backend/mps/mps_library.h"
#include "orteaf/internal/backend/mps/mps_fence.h"
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
    MOCK_METHOD(::orteaf::internal::backend::mps::MPSFence_t, createFence,
                (::orteaf::internal::backend::mps::MPSDevice_t));
    MOCK_METHOD(void, destroyFence,
                (::orteaf::internal::backend::mps::MPSFence_t));
    MOCK_METHOD(::orteaf::internal::backend::mps::MPSLibrary_t, createLibraryWithName,
                (::orteaf::internal::backend::mps::MPSDevice_t, std::string_view));
    MOCK_METHOD(void, destroyLibrary,
                (::orteaf::internal::backend::mps::MPSLibrary_t));
    MOCK_METHOD(::orteaf::internal::backend::mps::MPSFunction_t, createFunction,
                (::orteaf::internal::backend::mps::MPSLibrary_t, std::string_view));
    MOCK_METHOD(void, destroyFunction,
                (::orteaf::internal::backend::mps::MPSFunction_t));
    MOCK_METHOD(::orteaf::internal::backend::mps::MPSComputePipelineState_t, createComputePipelineState,
                (::orteaf::internal::backend::mps::MPSDevice_t,
                 ::orteaf::internal::backend::mps::MPSFunction_t));
    MOCK_METHOD(void, destroyComputePipelineState,
                (::orteaf::internal::backend::mps::MPSComputePipelineState_t));
    MOCK_METHOD(::orteaf::internal::backend::mps::MPSHeapDescriptor_t, createHeapDescriptor, ());
    MOCK_METHOD(void, destroyHeapDescriptor,
                (::orteaf::internal::backend::mps::MPSHeapDescriptor_t));
    MOCK_METHOD(void, setHeapDescriptorSize,
                (::orteaf::internal::backend::mps::MPSHeapDescriptor_t, std::size_t));
    MOCK_METHOD(void, setHeapDescriptorResourceOptions,
                (::orteaf::internal::backend::mps::MPSHeapDescriptor_t,
                 ::orteaf::internal::backend::mps::MPSResourceOptions_t));
    MOCK_METHOD(void, setHeapDescriptorStorageMode,
                (::orteaf::internal::backend::mps::MPSHeapDescriptor_t,
                 ::orteaf::internal::backend::mps::MPSStorageMode_t));
    MOCK_METHOD(void, setHeapDescriptorCPUCacheMode,
                (::orteaf::internal::backend::mps::MPSHeapDescriptor_t,
                 ::orteaf::internal::backend::mps::MPSCPUCacheMode_t));
    MOCK_METHOD(void, setHeapDescriptorHazardTrackingMode,
                (::orteaf::internal::backend::mps::MPSHeapDescriptor_t,
                 ::orteaf::internal::backend::mps::MPSHazardTrackingMode_t));
    MOCK_METHOD(void, setHeapDescriptorType,
                (::orteaf::internal::backend::mps::MPSHeapDescriptor_t,
                 ::orteaf::internal::backend::mps::MPSHeapType_t));
    MOCK_METHOD(::orteaf::internal::backend::mps::MPSHeap_t, createHeap,
                (::orteaf::internal::backend::mps::MPSDevice_t,
                 ::orteaf::internal::backend::mps::MPSHeapDescriptor_t));
    MOCK_METHOD(void, destroyHeap,
                (::orteaf::internal::backend::mps::MPSHeap_t));
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

    static ::orteaf::internal::backend::mps::MPSFence_t createFence(
            ::orteaf::internal::backend::mps::MPSDevice_t device) {
        return MpsBackendOpsMockRegistry::get().createFence(device);
    }

    static void destroyFence(::orteaf::internal::backend::mps::MPSFence_t fence) {
        MpsBackendOpsMockRegistry::get().destroyFence(fence);
    }

    static ::orteaf::internal::backend::mps::MPSLibrary_t createLibraryWithName(
            ::orteaf::internal::backend::mps::MPSDevice_t device,
            std::string_view name) {
        return MpsBackendOpsMockRegistry::get().createLibraryWithName(device, name);
    }

    static void destroyLibrary(::orteaf::internal::backend::mps::MPSLibrary_t library) {
        MpsBackendOpsMockRegistry::get().destroyLibrary(library);
    }

    static ::orteaf::internal::backend::mps::MPSFunction_t createFunction(
            ::orteaf::internal::backend::mps::MPSLibrary_t library,
            std::string_view name) {
        return MpsBackendOpsMockRegistry::get().createFunction(library, name);
    }

    static void destroyFunction(::orteaf::internal::backend::mps::MPSFunction_t function) {
        MpsBackendOpsMockRegistry::get().destroyFunction(function);
    }

    static ::orteaf::internal::backend::mps::MPSComputePipelineState_t createComputePipelineState(
            ::orteaf::internal::backend::mps::MPSDevice_t device,
            ::orteaf::internal::backend::mps::MPSFunction_t function) {
        return MpsBackendOpsMockRegistry::get().createComputePipelineState(device, function);
    }

    static void destroyComputePipelineState(
            ::orteaf::internal::backend::mps::MPSComputePipelineState_t pipeline_state) {
        MpsBackendOpsMockRegistry::get().destroyComputePipelineState(pipeline_state);
    }

    static ::orteaf::internal::backend::mps::MPSHeapDescriptor_t createHeapDescriptor() {
        return MpsBackendOpsMockRegistry::get().createHeapDescriptor();
    }

    static void destroyHeapDescriptor(::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor) {
        MpsBackendOpsMockRegistry::get().destroyHeapDescriptor(descriptor);
    }

    static void setHeapDescriptorSize(
            ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor, std::size_t size) {
        MpsBackendOpsMockRegistry::get().setHeapDescriptorSize(descriptor, size);
    }

    static void setHeapDescriptorResourceOptions(
            ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
            ::orteaf::internal::backend::mps::MPSResourceOptions_t options) {
        MpsBackendOpsMockRegistry::get().setHeapDescriptorResourceOptions(descriptor, options);
    }

    static void setHeapDescriptorStorageMode(
            ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
            ::orteaf::internal::backend::mps::MPSStorageMode_t storage_mode) {
        MpsBackendOpsMockRegistry::get().setHeapDescriptorStorageMode(descriptor, storage_mode);
    }

    static void setHeapDescriptorCPUCacheMode(
            ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
            ::orteaf::internal::backend::mps::MPSCPUCacheMode_t cache_mode) {
        MpsBackendOpsMockRegistry::get().setHeapDescriptorCPUCacheMode(descriptor, cache_mode);
    }

    static void setHeapDescriptorHazardTrackingMode(
            ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
            ::orteaf::internal::backend::mps::MPSHazardTrackingMode_t hazard_mode) {
        MpsBackendOpsMockRegistry::get().setHeapDescriptorHazardTrackingMode(descriptor, hazard_mode);
    }

    static void setHeapDescriptorType(
            ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
            ::orteaf::internal::backend::mps::MPSHeapType_t type) {
        MpsBackendOpsMockRegistry::get().setHeapDescriptorType(descriptor, type);
    }

    static ::orteaf::internal::backend::mps::MPSHeap_t createHeap(
            ::orteaf::internal::backend::mps::MPSDevice_t device,
            ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor) {
        return MpsBackendOpsMockRegistry::get().createHeap(device, descriptor);
    }

    static void destroyHeap(::orteaf::internal::backend::mps::MPSHeap_t heap) {
        MpsBackendOpsMockRegistry::get().destroyHeap(heap);
    }
};

}  // namespace orteaf::tests::runtime::mps

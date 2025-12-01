#pragma once

#include <string_view>

#include <gmock/gmock.h>

#include <orteaf/internal/backend/mps/wrapper/mps_command_queue.h>
#include <orteaf/internal/backend/mps/wrapper/mps_compute_pipeline_state.h>
#include <orteaf/internal/backend/mps/wrapper/mps_event.h>
#include <orteaf/internal/backend/mps/wrapper/mps_fence.h>
#include <orteaf/internal/backend/mps/wrapper/mps_function.h>
#include <orteaf/internal/backend/mps/wrapper/mps_heap.h>
#include <orteaf/internal/backend/mps/wrapper/mps_library.h>
#include <orteaf/internal/base/handle.h>
#include <orteaf/internal/backend/mps/mps_slow_ops.h>
#include <tests/internal/testing/static_mock.h>

namespace orteaf::tests::runtime::mps {

struct MpsBackendOpsMock
    : public orteaf::internal::runtime::backend_ops::mps::MpsSlowOps {
  MOCK_METHOD(int, getDeviceCount, (), (override));
  MOCK_METHOD(::orteaf::internal::backend::mps::MPSDevice_t, getDevice,
              (::orteaf::internal::backend::mps::MPSInt_t), (override));
  MOCK_METHOD(void, releaseDevice,
              (::orteaf::internal::backend::mps::MPSDevice_t), (override));
  MOCK_METHOD(::orteaf::internal::architecture::Architecture,
              detectArchitecture, (::orteaf::internal::base::DeviceId),
              (override));
  MOCK_METHOD(::orteaf::internal::backend::mps::MPSCommandQueue_t,
              createCommandQueue,
              (::orteaf::internal::backend::mps::MPSDevice_t), (override));
  MOCK_METHOD(void, destroyCommandQueue,
              (::orteaf::internal::backend::mps::MPSCommandQueue_t),
              (override));
  MOCK_METHOD(::orteaf::internal::backend::mps::MPSEvent_t, createEvent,
              (::orteaf::internal::backend::mps::MPSDevice_t), (override));
  MOCK_METHOD(void, destroyEvent,
              (::orteaf::internal::backend::mps::MPSEvent_t), (override));
  MOCK_METHOD(::orteaf::internal::backend::mps::MPSFence_t, createFence,
              (::orteaf::internal::backend::mps::MPSDevice_t), (override));
  MOCK_METHOD(void, destroyFence,
              (::orteaf::internal::backend::mps::MPSFence_t), (override));
  MOCK_METHOD(::orteaf::internal::backend::mps::MPSLibrary_t,
              createLibraryWithName,
              (::orteaf::internal::backend::mps::MPSDevice_t, std::string_view),
              (override));
  MOCK_METHOD(void, destroyLibrary,
              (::orteaf::internal::backend::mps::MPSLibrary_t), (override));
  MOCK_METHOD(::orteaf::internal::backend::mps::MPSFunction_t, createFunction,
              (::orteaf::internal::backend::mps::MPSLibrary_t,
               std::string_view),
              (override));
  MOCK_METHOD(void, destroyFunction,
              (::orteaf::internal::backend::mps::MPSFunction_t), (override));
  MOCK_METHOD(::orteaf::internal::backend::mps::MPSComputePipelineState_t,
              createComputePipelineState,
              (::orteaf::internal::backend::mps::MPSDevice_t,
               ::orteaf::internal::backend::mps::MPSFunction_t),
              (override));
  MOCK_METHOD(void, destroyComputePipelineState,
              (::orteaf::internal::backend::mps::MPSComputePipelineState_t),
              (override));
  MOCK_METHOD(::orteaf::internal::backend::mps::MPSHeapDescriptor_t,
              createHeapDescriptor, (), (override));
  MOCK_METHOD(void, destroyHeapDescriptor,
              (::orteaf::internal::backend::mps::MPSHeapDescriptor_t),
              (override));
  MOCK_METHOD(void, setHeapDescriptorSize,
              (::orteaf::internal::backend::mps::MPSHeapDescriptor_t,
               std::size_t),
              (override));
  MOCK_METHOD(void, setHeapDescriptorResourceOptions,
              (::orteaf::internal::backend::mps::MPSHeapDescriptor_t,
               ::orteaf::internal::backend::mps::MPSResourceOptions_t),
              (override));
  MOCK_METHOD(void, setHeapDescriptorStorageMode,
              (::orteaf::internal::backend::mps::MPSHeapDescriptor_t,
               ::orteaf::internal::backend::mps::MPSStorageMode_t),
              (override));
  MOCK_METHOD(void, setHeapDescriptorCPUCacheMode,
              (::orteaf::internal::backend::mps::MPSHeapDescriptor_t,
               ::orteaf::internal::backend::mps::MPSCPUCacheMode_t),
              (override));
  MOCK_METHOD(void, setHeapDescriptorHazardTrackingMode,
              (::orteaf::internal::backend::mps::MPSHeapDescriptor_t,
               ::orteaf::internal::backend::mps::MPSHazardTrackingMode_t),
              (override));
  MOCK_METHOD(void, setHeapDescriptorType,
              (::orteaf::internal::backend::mps::MPSHeapDescriptor_t,
               ::orteaf::internal::backend::mps::MPSHeapType_t),
              (override));
  MOCK_METHOD(::orteaf::internal::backend::mps::MPSHeap_t, createHeap,
              (::orteaf::internal::backend::mps::MPSDevice_t,
               ::orteaf::internal::backend::mps::MPSHeapDescriptor_t),
              (override));
  MOCK_METHOD(void, destroyHeap, (::orteaf::internal::backend::mps::MPSHeap_t),
              (override));
};

// Note: MpsBackendOpsMockAdapter is removed as we are moving to instance-based
// mocks. Tests should instantiate MpsBackendOpsMock directly.

} // namespace orteaf::tests::runtime::mps

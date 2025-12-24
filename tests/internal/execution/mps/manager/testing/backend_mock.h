#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <gmock/gmock.h>

#include <orteaf/internal/base/handle.h>
#include <orteaf/internal/execution/mps/platform/mps_slow_ops.h>
#include <orteaf/internal/execution/mps/platform/wrapper/mps_command_queue.h>
#include <orteaf/internal/execution/mps/platform/wrapper/mps_compute_pipeline_state.h>
#include <orteaf/internal/execution/mps/platform/wrapper/mps_event.h>
#include <orteaf/internal/execution/mps/platform/wrapper/mps_fence.h>
#include <orteaf/internal/execution/mps/platform/wrapper/mps_function.h>
#include <orteaf/internal/execution/mps/platform/wrapper/mps_heap.h>
#include <orteaf/internal/execution/mps/platform/wrapper/mps_library.h>
#include <tests/internal/testing/static_mock.h>

namespace orteaf::tests::execution::mps {

struct MpsBackendOpsMock
    : public orteaf::internal::execution::mps::platform::MpsSlowOps {
  MOCK_METHOD(int, getDeviceCount, (), (override));
  MOCK_METHOD(::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t,
              getDevice,
              (::orteaf::internal::execution::mps::platform::wrapper::MPSInt_t),
              (override));
  MOCK_METHOD(
      void, releaseDevice,
      (::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t),
      (override));
  MOCK_METHOD(::orteaf::internal::architecture::Architecture,
              detectArchitecture, (::orteaf::internal::base::DeviceHandle),
              (override));
  MOCK_METHOD(
      ::orteaf::internal::execution::mps::platform::wrapper::MpsCommandQueue_t,
      createCommandQueue,
      (::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t),
      (override));
  MOCK_METHOD(
      void, destroyCommandQueue,
      (::orteaf::internal::execution::mps::platform::wrapper::MpsCommandQueue_t),
      (override));
  MOCK_METHOD(
      ::orteaf::internal::execution::mps::platform::wrapper::MpsEvent_t,
      createEvent,
      (::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t),
      (override));
  MOCK_METHOD(void, destroyEvent,
              (::orteaf::internal::execution::mps::platform::wrapper::MpsEvent_t),
              (override));
  MOCK_METHOD(
      ::orteaf::internal::execution::mps::platform::wrapper::MpsFence_t,
      createFence,
      (::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t),
      (override));
  MOCK_METHOD(void, destroyFence,
              (::orteaf::internal::execution::mps::platform::wrapper::MpsFence_t),
              (override));
  MOCK_METHOD(::orteaf::internal::execution::mps::platform::wrapper::MpsLibrary_t,
              createLibraryWithName,
              (::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t,
               std::string_view),
              (override));
  MOCK_METHOD(
      void, destroyLibrary,
      (::orteaf::internal::execution::mps::platform::wrapper::MpsLibrary_t),
      (override));
  MOCK_METHOD(
      ::orteaf::internal::execution::mps::platform::wrapper::MpsFunction_t,
      createFunction,
      (::orteaf::internal::execution::mps::platform::wrapper::MpsLibrary_t,
       std::string_view),
      (override));
  MOCK_METHOD(
      void, destroyFunction,
      (::orteaf::internal::execution::mps::platform::wrapper::MpsFunction_t),
      (override));
  MOCK_METHOD(
      ::orteaf::internal::execution::mps::platform::wrapper::
          MpsComputePipelineState_t,
      createComputePipelineState,
      (::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t,
       ::orteaf::internal::execution::mps::platform::wrapper::MpsFunction_t),
      (override));
  MOCK_METHOD(void, destroyComputePipelineState,
              (::orteaf::internal::execution::mps::platform::wrapper::
                   MpsComputePipelineState_t),
              (override));
  MOCK_METHOD(
      ::orteaf::internal::execution::mps::platform::wrapper::MpsHeapDescriptor_t,
      createHeapDescriptor, (), (override));
  MOCK_METHOD(void, destroyHeapDescriptor,
              (::orteaf::internal::execution::mps::platform::wrapper::
                   MpsHeapDescriptor_t),
              (override));
  MOCK_METHOD(
      void, setHeapDescriptorSize,
      (::orteaf::internal::execution::mps::platform::wrapper::MpsHeapDescriptor_t,
       std::size_t),
      (override));
  MOCK_METHOD(
      void, setHeapDescriptorResourceOptions,
      (::orteaf::internal::execution::mps::platform::wrapper::MpsHeapDescriptor_t,
       ::orteaf::internal::execution::mps::platform::wrapper::
           MpsResourceOptions_t),
      (override));
  MOCK_METHOD(
      void, setHeapDescriptorStorageMode,
      (::orteaf::internal::execution::mps::platform::wrapper::MpsHeapDescriptor_t,
       ::orteaf::internal::execution::mps::platform::wrapper::MpsStorageMode_t),
      (override));
  MOCK_METHOD(
      void, setHeapDescriptorCPUCacheMode,
      (::orteaf::internal::execution::mps::platform::wrapper::MpsHeapDescriptor_t,
       ::orteaf::internal::execution::mps::platform::wrapper::MpsCPUCacheMode_t),
      (override));
  MOCK_METHOD(
      void, setHeapDescriptorHazardTrackingMode,
      (::orteaf::internal::execution::mps::platform::wrapper::MpsHeapDescriptor_t,
       ::orteaf::internal::execution::mps::platform::wrapper::
           MpsHazardTrackingMode_t),
      (override));
  MOCK_METHOD(
      void, setHeapDescriptorType,
      (::orteaf::internal::execution::mps::platform::wrapper::MpsHeapDescriptor_t,
       ::orteaf::internal::execution::mps::platform::wrapper::MpsHeapType_t),
      (override));
  MOCK_METHOD(::orteaf::internal::execution::mps::platform::wrapper::MpsHeap_t,
              createHeap,
              (::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t,
               ::orteaf::internal::execution::mps::platform::wrapper::
                   MpsHeapDescriptor_t),
              (override));
  MOCK_METHOD(void, destroyHeap,
              (::orteaf::internal::execution::mps::platform::wrapper::MpsHeap_t),
              (override));
  MOCK_METHOD(::orteaf::internal::execution::mps::platform::wrapper::MpsGraph_t,
              createGraph, (), (override));
  MOCK_METHOD(void, destroyGraph,
              (::orteaf::internal::execution::mps::platform::wrapper::MpsGraph_t),
              (override));
  MOCK_METHOD(
      ::orteaf::internal::execution::mps::platform::wrapper::MpsGraphTensorData_t,
      createGraphTensorData,
      (::orteaf::internal::execution::mps::platform::wrapper::MpsBuffer_t,
       const std::int64_t *, std::size_t,
       ::orteaf::internal::execution::mps::platform::wrapper::MpsGraphDataType),
      (override));
  MOCK_METHOD(void, destroyGraphTensorData,
              (::orteaf::internal::execution::mps::platform::wrapper::
                   MpsGraphTensorData_t),
              (override));
  MOCK_METHOD(
      ::orteaf::internal::execution::mps::platform::wrapper::MpsGraphExecutable_t,
      compileGraph,
      (::orteaf::internal::execution::mps::platform::wrapper::MpsGraph_t,
       ::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t,
       const ::orteaf::internal::execution::mps::platform::wrapper::MpsGraphFeed
           *,
       std::size_t,
       const ::orteaf::internal::execution::mps::platform::wrapper::
           MpsGraphTensor_t *,
       std::size_t,
       const ::orteaf::internal::execution::mps::platform::wrapper::
           MpsGraphOperation_t *,
       std::size_t),
      (override));
  MOCK_METHOD(
      std::size_t, runGraphExecutable,
      (::orteaf::internal::execution::mps::platform::wrapper::
           MpsGraphExecutable_t,
       ::orteaf::internal::execution::mps::platform::wrapper::MpsCommandQueue_t,
       const ::orteaf::internal::execution::mps::platform::wrapper::MpsGraphFeed
           *,
       std::size_t,
       const ::orteaf::internal::execution::mps::platform::wrapper::
           MpsGraphTensor_t *,
       std::size_t,
       const ::orteaf::internal::execution::mps::platform::wrapper::
           MpsGraphOperation_t *,
       std::size_t,
       ::orteaf::internal::execution::mps::platform::wrapper::MpsGraphTensorData_t
           *,
       std::size_t),
      (override));
  MOCK_METHOD(void, destroyGraphExecutable,
              (::orteaf::internal::execution::mps::platform::wrapper::
                   MpsGraphExecutable_t),
              (override));
};

// Note: MpsBackendOpsMockAdapter is removed as we are moving to instance-based
// mocks. Tests should instantiate MpsBackendOpsMock directly.

} // namespace orteaf::tests::execution::mps

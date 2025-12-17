#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "orteaf/internal/architecture/architecture.h"
#include "orteaf/internal/architecture/mps_detect.h"
#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_command_queue.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_compute_pipeline_state.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_device.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_event.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_fence.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_function.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_graph.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_heap.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_library.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_string.h"

namespace orteaf::internal::runtime::mps::platform {

struct MpsSlowOps {
  virtual ~MpsSlowOps() = default;

  virtual int getDeviceCount() = 0;

  virtual ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t
  getDevice(
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSInt_t index) = 0;

  virtual void
  releaseDevice(::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t
                    device) = 0;

  virtual ::orteaf::internal::architecture::Architecture
  detectArchitecture(::orteaf::internal::base::DeviceHandle device_id) = 0;

  virtual ::orteaf::internal::runtime::mps::platform::wrapper::MpsCommandQueue_t
  createCommandQueue(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t
          device) = 0;

  virtual void destroyCommandQueue(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsCommandQueue_t
          queue) = 0;

  virtual ::orteaf::internal::runtime::mps::platform::wrapper::MpsEvent_t
  createEvent(::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t
                  device) = 0;

  virtual void
  destroyEvent(::orteaf::internal::runtime::mps::platform::wrapper::MpsEvent_t
                   event) = 0;

  virtual ::orteaf::internal::runtime::mps::platform::wrapper::MpsFence_t
  createFence(::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t
                  device) = 0;

  virtual void
  destroyFence(::orteaf::internal::runtime::mps::platform::wrapper::MpsFence_t
                   fence) = 0;

  virtual ::orteaf::internal::runtime::mps::platform::wrapper::MpsLibrary_t
  createLibraryWithName(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t device,
      std::string_view name) = 0;

  virtual void destroyLibrary(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsLibrary_t
          library) = 0;

  virtual ::orteaf::internal::runtime::mps::platform::wrapper::MpsFunction_t
  createFunction(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsLibrary_t library,
      std::string_view name) = 0;

  virtual void destroyFunction(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsFunction_t
          function) = 0;

  virtual ::orteaf::internal::runtime::mps::platform::wrapper::
      MpsComputePipelineState_t
      createComputePipelineState(
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t
              device,
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsFunction_t
              function) = 0;

  virtual void destroyComputePipelineState(
      ::orteaf::internal::runtime::mps::platform::wrapper::
          MpsComputePipelineState_t pipeline_state) = 0;

  virtual ::orteaf::internal::runtime::mps::platform::wrapper::
      MpsHeapDescriptor_t
      createHeapDescriptor() = 0;

  virtual void destroyHeapDescriptor(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapDescriptor_t
          descriptor) = 0;

  virtual void setHeapDescriptorSize(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapDescriptor_t
          descriptor,
      std::size_t size) = 0;

  virtual void setHeapDescriptorResourceOptions(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapDescriptor_t
          descriptor,
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsResourceOptions_t
          options) = 0;

  virtual void setHeapDescriptorStorageMode(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapDescriptor_t
          descriptor,
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsStorageMode_t
          storage_mode) = 0;

  virtual void setHeapDescriptorCPUCacheMode(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapDescriptor_t
          descriptor,
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsCPUCacheMode_t
          cpu_cache_mode) = 0;

  virtual void setHeapDescriptorHazardTrackingMode(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapDescriptor_t
          descriptor,
      ::orteaf::internal::runtime::mps::platform::wrapper::
          MpsHazardTrackingMode_t hazard_mode) = 0;

  virtual void setHeapDescriptorType(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapDescriptor_t
          descriptor,
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapType_t
          type) = 0;

  virtual ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeap_t
  createHeap(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t device,
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapDescriptor_t
          descriptor) = 0;

  virtual void destroyHeap(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeap_t heap) = 0;

  virtual ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraph_t
  createGraph() = 0;

  virtual void
  destroyGraph(::orteaf::internal::runtime::mps::platform::wrapper::MpsGraph_t
                   graph) = 0;

  virtual ::orteaf::internal::runtime::mps::platform::wrapper::
      MpsGraphTensorData_t
      createGraphTensorData(
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsBuffer_t
              buffer,
          const std::int64_t *shape, std::size_t shape_rank,
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphDataType
              data_type) = 0;

  virtual void destroyGraphTensorData(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphTensorData_t
          tensor_data) = 0;

  virtual ::orteaf::internal::runtime::mps::platform::wrapper::
      MpsGraphExecutable_t
      compileGraph(
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraph_t graph,
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t
              device,
          const ::orteaf::internal::runtime::mps::platform::wrapper::
              MpsGraphFeed *feeds,
          std::size_t feed_count,
          const ::orteaf::internal::runtime::mps::platform::wrapper::
              MpsGraphTensor_t *target_tensors,
          std::size_t target_tensor_count,
          const ::orteaf::internal::runtime::mps::platform::wrapper::
              MpsGraphOperation_t *target_operations,
          std::size_t target_operation_count) = 0;

  virtual std::size_t runGraphExecutable(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphExecutable_t
          executable,
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsCommandQueue_t
          command_queue,
      const ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphFeed
          *feeds,
      std::size_t feed_count,
      const ::orteaf::internal::runtime::mps::platform::wrapper::
          MpsGraphTensor_t *target_tensors,
      std::size_t target_tensor_count,
      const ::orteaf::internal::runtime::mps::platform::wrapper::
          MpsGraphOperation_t *target_operations,
      std::size_t target_operation_count,
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphTensorData_t
          *out_tensor_data,
      std::size_t out_capacity) = 0;

  virtual void destroyGraphExecutable(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphExecutable_t
          executable) = 0;
};

struct MpsSlowOpsImpl final : public MpsSlowOps {
  int getDeviceCount() override;

  ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t
  getDevice(::orteaf::internal::runtime::mps::platform::wrapper::MPSInt_t index)
      override;

  void releaseDevice(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t device)
      override;

  ::orteaf::internal::architecture::Architecture
  detectArchitecture(::orteaf::internal::base::DeviceHandle device_id) override;

  ::orteaf::internal::runtime::mps::platform::wrapper::MpsCommandQueue_t
  createCommandQueue(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t device)
      override;

  void destroyCommandQueue(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsCommandQueue_t
          queue) override;

  ::orteaf::internal::runtime::mps::platform::wrapper::MpsEvent_t createEvent(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t device)
      override;

  void destroyEvent(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsEvent_t event)
      override;

  ::orteaf::internal::runtime::mps::platform::wrapper::MpsFence_t createFence(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t device)
      override;

  void destroyFence(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsFence_t fence)
      override;

  ::orteaf::internal::runtime::mps::platform::wrapper::MpsLibrary_t
  createLibraryWithName(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t device,
      std::string_view name) override;

  void destroyLibrary(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsLibrary_t library)
      override;

  ::orteaf::internal::runtime::mps::platform::wrapper::MpsFunction_t
  createFunction(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsLibrary_t library,
      std::string_view name) override;

  void destroyFunction(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsFunction_t
          function) override;

  ::orteaf::internal::runtime::mps::platform::wrapper::MpsComputePipelineState_t
  createComputePipelineState(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t device,
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsFunction_t
          function) override;

  void destroyComputePipelineState(
      ::orteaf::internal::runtime::mps::platform::wrapper::
          MpsComputePipelineState_t pipeline_state) override;

  ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapDescriptor_t
  createHeapDescriptor() override;

  void destroyHeapDescriptor(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapDescriptor_t
          descriptor) override;

  void setHeapDescriptorSize(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapDescriptor_t
          descriptor,
      std::size_t size) override;

  void setHeapDescriptorResourceOptions(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapDescriptor_t
          descriptor,
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsResourceOptions_t
          options) override;

  void setHeapDescriptorStorageMode(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapDescriptor_t
          descriptor,
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsStorageMode_t
          storage_mode) override;

  void setHeapDescriptorCPUCacheMode(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapDescriptor_t
          descriptor,
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsCPUCacheMode_t
          cpu_cache_mode) override;

  void setHeapDescriptorHazardTrackingMode(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapDescriptor_t
          descriptor,
      ::orteaf::internal::runtime::mps::platform::wrapper::
          MpsHazardTrackingMode_t hazard_mode) override;

  void setHeapDescriptorType(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapDescriptor_t
          descriptor,
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapType_t type)
      override;

  ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeap_t createHeap(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t device,
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapDescriptor_t
          descriptor) override;

  void destroyHeap(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeap_t heap)
      override;

  ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraph_t
  createGraph() override;

  void destroyGraph(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraph_t graph)
      override;

  ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphTensorData_t
  createGraphTensorData(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsBuffer_t buffer,
      const std::int64_t *shape, std::size_t shape_rank,
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphDataType
          data_type) override;

  void destroyGraphTensorData(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphTensorData_t
          tensor_data) override;

  ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphExecutable_t
  compileGraph(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraph_t graph,
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t device,
      const ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphFeed
          *feeds,
      std::size_t feed_count,
      const ::orteaf::internal::runtime::mps::platform::wrapper::
          MpsGraphTensor_t *target_tensors,
      std::size_t target_tensor_count,
      const ::orteaf::internal::runtime::mps::platform::wrapper::
          MpsGraphOperation_t *target_operations,
      std::size_t target_operation_count) override;

  std::size_t runGraphExecutable(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphExecutable_t
          executable,
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsCommandQueue_t
          command_queue,
      const ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphFeed
          *feeds,
      std::size_t feed_count,
      const ::orteaf::internal::runtime::mps::platform::wrapper::
          MpsGraphTensor_t *target_tensors,
      std::size_t target_tensor_count,
      const ::orteaf::internal::runtime::mps::platform::wrapper::
          MpsGraphOperation_t *target_operations,
      std::size_t target_operation_count,
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphTensorData_t
          *out_tensor_data,
      std::size_t out_capacity) override;

  void destroyGraphExecutable(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphExecutable_t
          executable) override;
};

} // namespace orteaf::internal::runtime::mps::platform

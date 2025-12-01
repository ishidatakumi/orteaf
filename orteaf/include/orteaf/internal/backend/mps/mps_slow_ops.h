#pragma once

#include <cstddef>
#include <string_view>

#include "orteaf/internal/architecture/architecture.h"
#include "orteaf/internal/architecture/mps_detect.h"
#include "orteaf/internal/backend/mps/wrapper/mps_command_queue.h"
#include "orteaf/internal/backend/mps/wrapper/mps_compute_pipeline_state.h"
#include "orteaf/internal/backend/mps/wrapper/mps_device.h"
#include "orteaf/internal/backend/mps/wrapper/mps_event.h"
#include "orteaf/internal/backend/mps/wrapper/mps_fence.h"
#include "orteaf/internal/backend/mps/wrapper/mps_function.h"
#include "orteaf/internal/backend/mps/wrapper/mps_heap.h"
#include "orteaf/internal/backend/mps/wrapper/mps_library.h"
#include "orteaf/internal/backend/mps/wrapper/mps_string.h"
#include "orteaf/internal/base/handle.h"

namespace orteaf::internal::runtime::backend_ops::mps {

struct MpsSlowOps {
  virtual ~MpsSlowOps() = default;

  virtual int getDeviceCount() = 0;

  virtual ::orteaf::internal::backend::mps::MPSDevice_t
  getDevice(::orteaf::internal::backend::mps::MPSInt_t index) = 0;

  virtual void
  releaseDevice(::orteaf::internal::backend::mps::MPSDevice_t device) = 0;

  virtual ::orteaf::internal::architecture::Architecture
  detectArchitecture(::orteaf::internal::base::DeviceId device_id) = 0;

  virtual ::orteaf::internal::backend::mps::MPSCommandQueue_t
  createCommandQueue(::orteaf::internal::backend::mps::MPSDevice_t device) = 0;

  virtual void destroyCommandQueue(
      ::orteaf::internal::backend::mps::MPSCommandQueue_t queue) = 0;

  virtual ::orteaf::internal::backend::mps::MPSEvent_t
  createEvent(::orteaf::internal::backend::mps::MPSDevice_t device) = 0;

  virtual void
  destroyEvent(::orteaf::internal::backend::mps::MPSEvent_t event) = 0;

  virtual ::orteaf::internal::backend::mps::MPSFence_t
  createFence(::orteaf::internal::backend::mps::MPSDevice_t device) = 0;

  virtual void
  destroyFence(::orteaf::internal::backend::mps::MPSFence_t fence) = 0;

  virtual ::orteaf::internal::backend::mps::MPSLibrary_t
  createLibraryWithName(::orteaf::internal::backend::mps::MPSDevice_t device,
                        std::string_view name) = 0;

  virtual void
  destroyLibrary(::orteaf::internal::backend::mps::MPSLibrary_t library) = 0;

  virtual ::orteaf::internal::backend::mps::MPSFunction_t
  createFunction(::orteaf::internal::backend::mps::MPSLibrary_t library,
                 std::string_view name) = 0;

  virtual void
  destroyFunction(::orteaf::internal::backend::mps::MPSFunction_t function) = 0;

  virtual ::orteaf::internal::backend::mps::MPSComputePipelineState_t
  createComputePipelineState(
      ::orteaf::internal::backend::mps::MPSDevice_t device,
      ::orteaf::internal::backend::mps::MPSFunction_t function) = 0;

  virtual void destroyComputePipelineState(
      ::orteaf::internal::backend::mps::MPSComputePipelineState_t
          pipeline_state) = 0;

  virtual ::orteaf::internal::backend::mps::MPSHeapDescriptor_t
  createHeapDescriptor() = 0;

  virtual void destroyHeapDescriptor(
      ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor) = 0;

  virtual void setHeapDescriptorSize(
      ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
      std::size_t size) = 0;

  virtual void setHeapDescriptorResourceOptions(
      ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
      ::orteaf::internal::backend::mps::MPSResourceOptions_t options) = 0;

  virtual void setHeapDescriptorStorageMode(
      ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
      ::orteaf::internal::backend::mps::MPSStorageMode_t storage_mode) = 0;

  virtual void setHeapDescriptorCPUCacheMode(
      ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
      ::orteaf::internal::backend::mps::MPSCPUCacheMode_t cpu_cache_mode) = 0;

  virtual void setHeapDescriptorHazardTrackingMode(
      ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
      ::orteaf::internal::backend::mps::MPSHazardTrackingMode_t
          hazard_mode) = 0;

  virtual void setHeapDescriptorType(
      ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
      ::orteaf::internal::backend::mps::MPSHeapType_t type) = 0;

  virtual ::orteaf::internal::backend::mps::MPSHeap_t createHeap(
      ::orteaf::internal::backend::mps::MPSDevice_t device,
      ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor) = 0;

  virtual void
  destroyHeap(::orteaf::internal::backend::mps::MPSHeap_t heap) = 0;
};

struct MpsSlowOpsImpl final : public MpsSlowOps {
  int getDeviceCount() override;

  ::orteaf::internal::backend::mps::MPSDevice_t
  getDevice(::orteaf::internal::backend::mps::MPSInt_t index) override;

  void releaseDevice(
      ::orteaf::internal::backend::mps::MPSDevice_t device) override;

  ::orteaf::internal::architecture::Architecture
  detectArchitecture(::orteaf::internal::base::DeviceId device_id) override;

  ::orteaf::internal::backend::mps::MPSCommandQueue_t
  createCommandQueue(
      ::orteaf::internal::backend::mps::MPSDevice_t device) override;

  void destroyCommandQueue(
      ::orteaf::internal::backend::mps::MPSCommandQueue_t queue) override;

  ::orteaf::internal::backend::mps::MPSEvent_t
  createEvent(::orteaf::internal::backend::mps::MPSDevice_t device) override;

  void destroyEvent(
      ::orteaf::internal::backend::mps::MPSEvent_t event) override;

  ::orteaf::internal::backend::mps::MPSFence_t
  createFence(::orteaf::internal::backend::mps::MPSDevice_t device) override;

  void destroyFence(
      ::orteaf::internal::backend::mps::MPSFence_t fence) override;

  ::orteaf::internal::backend::mps::MPSLibrary_t
  createLibraryWithName(::orteaf::internal::backend::mps::MPSDevice_t device,
                        std::string_view name) override;

  void destroyLibrary(
      ::orteaf::internal::backend::mps::MPSLibrary_t library) override;

  ::orteaf::internal::backend::mps::MPSFunction_t
  createFunction(::orteaf::internal::backend::mps::MPSLibrary_t library,
                 std::string_view name) override;

  void destroyFunction(
      ::orteaf::internal::backend::mps::MPSFunction_t function) override;

  ::orteaf::internal::backend::mps::MPSComputePipelineState_t
  createComputePipelineState(
      ::orteaf::internal::backend::mps::MPSDevice_t device,
      ::orteaf::internal::backend::mps::MPSFunction_t function) override;

  void destroyComputePipelineState(
      ::orteaf::internal::backend::mps::MPSComputePipelineState_t
          pipeline_state) override;

  ::orteaf::internal::backend::mps::MPSHeapDescriptor_t
  createHeapDescriptor() override;

  void destroyHeapDescriptor(
      ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor)
      override;

  void setHeapDescriptorSize(
      ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
      std::size_t size) override;

  void setHeapDescriptorResourceOptions(
      ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
      ::orteaf::internal::backend::mps::MPSResourceOptions_t options) override;

  void setHeapDescriptorStorageMode(
      ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
      ::orteaf::internal::backend::mps::MPSStorageMode_t storage_mode)
      override;

  void setHeapDescriptorCPUCacheMode(
      ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
      ::orteaf::internal::backend::mps::MPSCPUCacheMode_t cpu_cache_mode)
      override;

  void setHeapDescriptorHazardTrackingMode(
      ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
      ::orteaf::internal::backend::mps::MPSHazardTrackingMode_t hazard_mode)
      override;

  void setHeapDescriptorType(
      ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
      ::orteaf::internal::backend::mps::MPSHeapType_t type) override;

  ::orteaf::internal::backend::mps::MPSHeap_t
  createHeap(::orteaf::internal::backend::mps::MPSDevice_t device,
             ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor)
      override;

  void destroyHeap(::orteaf::internal::backend::mps::MPSHeap_t heap) override;
};

} // namespace orteaf::internal::runtime::backend_ops::mps

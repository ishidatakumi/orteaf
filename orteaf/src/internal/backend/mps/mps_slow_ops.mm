#include "orteaf/internal/backend/mps/mps_slow_ops.h"
#include "orteaf/internal/backend/mps/wrapper/metal_kernel_embed_api.h"
#include "orteaf/internal/backend/mps/wrapper/mps_objc_bridge.h"

namespace orteaf::internal::runtime::backend_ops::mps {

int MpsSlowOpsImpl::getDeviceCount() {
  return ::orteaf::internal::backend::mps::getDeviceCount();
}

::orteaf::internal::backend::mps::MPSDevice_t
MpsSlowOpsImpl::getDevice(::orteaf::internal::backend::mps::MPSInt_t index) {
  return ::orteaf::internal::backend::mps::getDevice(index);
}

void MpsSlowOpsImpl::releaseDevice(
    ::orteaf::internal::backend::mps::MPSDevice_t device) {
  ::orteaf::internal::backend::mps::deviceRelease(device);
}

::orteaf::internal::architecture::Architecture
MpsSlowOpsImpl::detectArchitecture(
    ::orteaf::internal::base::DeviceHandle device_id) {
  return ::orteaf::internal::architecture::detectMpsArchitectureForDeviceId(
      device_id);
}

::orteaf::internal::backend::mps::MPSCommandQueue_t
MpsSlowOpsImpl::createCommandQueue(
    ::orteaf::internal::backend::mps::MPSDevice_t device) {
  return ::orteaf::internal::backend::mps::createCommandQueue(device);
}

void MpsSlowOpsImpl::destroyCommandQueue(
    ::orteaf::internal::backend::mps::MPSCommandQueue_t queue) {
  ::orteaf::internal::backend::mps::destroyCommandQueue(queue);
}

::orteaf::internal::backend::mps::MPSEvent_t
MpsSlowOpsImpl::createEvent(
    ::orteaf::internal::backend::mps::MPSDevice_t device) {
  return ::orteaf::internal::backend::mps::createEvent(device);
}

void MpsSlowOpsImpl::destroyEvent(
    ::orteaf::internal::backend::mps::MPSEvent_t event) {
  ::orteaf::internal::backend::mps::destroyEvent(event);
}

::orteaf::internal::backend::mps::MPSFence_t
MpsSlowOpsImpl::createFence(
    ::orteaf::internal::backend::mps::MPSDevice_t device) {
  return ::orteaf::internal::backend::mps::createFence(device);
}

void MpsSlowOpsImpl::destroyFence(
    ::orteaf::internal::backend::mps::MPSFence_t fence) {
  ::orteaf::internal::backend::mps::destroyFence(fence);
}

::orteaf::internal::backend::mps::MPSLibrary_t
MpsSlowOpsImpl::createLibraryWithName(
    ::orteaf::internal::backend::mps::MPSDevice_t device,
    std::string_view name) {
  auto library = ::orteaf::internal::backend::mps::metal_kernel_embed::
      createEmbeddedLibrary(device, name, nullptr);
  if (library != nullptr) {
    return library;
  }
  auto ns_name = ::orteaf::internal::backend::mps::toNsString(name);
  library = ::orteaf::internal::backend::mps::createLibrary(device, ns_name, nullptr);
  opaqueReleaseRetained(ns_name);
  return library;
}

void MpsSlowOpsImpl::destroyLibrary(
    ::orteaf::internal::backend::mps::MPSLibrary_t library) {
  ::orteaf::internal::backend::mps::destroyLibrary(library);
}

::orteaf::internal::backend::mps::MPSFunction_t
MpsSlowOpsImpl::createFunction(
    ::orteaf::internal::backend::mps::MPSLibrary_t library,
    std::string_view name) {
  return ::orteaf::internal::backend::mps::createFunction(library, name);
}

void MpsSlowOpsImpl::destroyFunction(
    ::orteaf::internal::backend::mps::MPSFunction_t function) {
  ::orteaf::internal::backend::mps::destroyFunction(function);
}

::orteaf::internal::backend::mps::MPSComputePipelineState_t
MpsSlowOpsImpl::createComputePipelineState(
    ::orteaf::internal::backend::mps::MPSDevice_t device,
    ::orteaf::internal::backend::mps::MPSFunction_t function) {
  return ::orteaf::internal::backend::mps::createComputePipelineState(device,
                                                                      function);
}

void MpsSlowOpsImpl::destroyComputePipelineState(
    ::orteaf::internal::backend::mps::MPSComputePipelineState_t pipeline_state) {
  ::orteaf::internal::backend::mps::destroyComputePipelineState(
      pipeline_state);
}

::orteaf::internal::backend::mps::MPSHeapDescriptor_t
MpsSlowOpsImpl::createHeapDescriptor() {
  return ::orteaf::internal::backend::mps::createHeapDescriptor();
}

void MpsSlowOpsImpl::destroyHeapDescriptor(
    ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor) {
  ::orteaf::internal::backend::mps::destroyHeapDescriptor(descriptor);
}

void MpsSlowOpsImpl::setHeapDescriptorSize(
    ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
    std::size_t size) {
  ::orteaf::internal::backend::mps::setHeapDescriptorSize(descriptor, size);
}

void MpsSlowOpsImpl::setHeapDescriptorResourceOptions(
    ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
    ::orteaf::internal::backend::mps::MPSResourceOptions_t options) {
  ::orteaf::internal::backend::mps::setHeapDescriptorResourceOptions(
      descriptor, options);
}

void MpsSlowOpsImpl::setHeapDescriptorStorageMode(
    ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
    ::orteaf::internal::backend::mps::MPSStorageMode_t storage_mode) {
  ::orteaf::internal::backend::mps::setHeapDescriptorStorageMode(
      descriptor, storage_mode);
}

void MpsSlowOpsImpl::setHeapDescriptorCPUCacheMode(
    ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
    ::orteaf::internal::backend::mps::MPSCPUCacheMode_t cpu_cache_mode) {
  ::orteaf::internal::backend::mps::setHeapDescriptorCPUCacheMode(
      descriptor, cpu_cache_mode);
}

void MpsSlowOpsImpl::setHeapDescriptorHazardTrackingMode(
    ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
    ::orteaf::internal::backend::mps::MPSHazardTrackingMode_t hazard_mode) {
  ::orteaf::internal::backend::mps::setHeapDescriptorHazardTrackingMode(
      descriptor, hazard_mode);
}

void MpsSlowOpsImpl::setHeapDescriptorType(
    ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor,
    ::orteaf::internal::backend::mps::MPSHeapType_t type) {
  ::orteaf::internal::backend::mps::setHeapDescriptorType(descriptor, type);
}

::orteaf::internal::backend::mps::MPSHeap_t MpsSlowOpsImpl::createHeap(
    ::orteaf::internal::backend::mps::MPSDevice_t device,
    ::orteaf::internal::backend::mps::MPSHeapDescriptor_t descriptor) {
  return ::orteaf::internal::backend::mps::createHeap(device, descriptor);
}

void MpsSlowOpsImpl::destroyHeap(
    ::orteaf::internal::backend::mps::MPSHeap_t heap) {
  ::orteaf::internal::backend::mps::destroyHeap(heap);
}

} // namespace orteaf::internal::runtime::backend_ops::mps

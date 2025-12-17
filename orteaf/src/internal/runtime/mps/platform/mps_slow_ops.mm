#include "orteaf/internal/runtime/mps/platform/mps_slow_ops.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/metal_kernel_embed_api.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_objc_bridge.h"

namespace orteaf::internal::runtime::mps::platform {

int MpsSlowOpsImpl::getDeviceCount() {
  return ::orteaf::internal::runtime::mps::platform::wrapper::getDeviceCount();
}

::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t
MpsSlowOpsImpl::getDevice(
    ::orteaf::internal::runtime::mps::platform::wrapper::MPSInt_t index) {
  return ::orteaf::internal::runtime::mps::platform::wrapper::getDevice(index);
}

void MpsSlowOpsImpl::releaseDevice(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t device) {
  ::orteaf::internal::runtime::mps::platform::wrapper::deviceRelease(device);
}

::orteaf::internal::architecture::Architecture
MpsSlowOpsImpl::detectArchitecture(
    ::orteaf::internal::base::DeviceHandle device_id) {
  return ::orteaf::internal::architecture::detectMpsArchitectureForDeviceId(
      device_id);
}

::orteaf::internal::runtime::mps::platform::wrapper::MpsCommandQueue_t
MpsSlowOpsImpl::createCommandQueue(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t device) {
  return ::orteaf::internal::runtime::mps::platform::wrapper::
      createCommandQueue(device);
}

void MpsSlowOpsImpl::destroyCommandQueue(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsCommandQueue_t
        queue) {
  ::orteaf::internal::runtime::mps::platform::wrapper::destroyCommandQueue(
      queue);
}

::orteaf::internal::runtime::mps::platform::wrapper::MpsEvent_t
MpsSlowOpsImpl::createEvent(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t device) {
  return ::orteaf::internal::runtime::mps::platform::wrapper::createEvent(
      device);
}

void MpsSlowOpsImpl::destroyEvent(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsEvent_t event) {
  ::orteaf::internal::runtime::mps::platform::wrapper::destroyEvent(event);
}

::orteaf::internal::runtime::mps::platform::wrapper::MpsFence_t
MpsSlowOpsImpl::createFence(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t device) {
  return ::orteaf::internal::runtime::mps::platform::wrapper::createFence(
      device);
}

void MpsSlowOpsImpl::destroyFence(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsFence_t fence) {
  ::orteaf::internal::runtime::mps::platform::wrapper::destroyFence(fence);
}

::orteaf::internal::runtime::mps::platform::wrapper::MpsLibrary_t
MpsSlowOpsImpl::createLibraryWithName(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t device,
    std::string_view name) {
  auto library = ::orteaf::internal::runtime::mps::platform::
      metal_kernel_embed::createEmbeddedLibrary(device, name, nullptr);
  if (library != nullptr) {
    return library;
  }
  auto ns_name =
      ::orteaf::internal::runtime::mps::platform::wrapper::toNsString(name);
  library = ::orteaf::internal::runtime::mps::platform::wrapper::createLibrary(
      device, ns_name, nullptr);
  opaqueReleaseRetained(ns_name);
  return library;
}

void MpsSlowOpsImpl::destroyLibrary(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsLibrary_t library) {
  ::orteaf::internal::runtime::mps::platform::wrapper::destroyLibrary(library);
}

::orteaf::internal::runtime::mps::platform::wrapper::MpsFunction_t
MpsSlowOpsImpl::createFunction(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsLibrary_t library,
    std::string_view name) {
  return ::orteaf::internal::runtime::mps::platform::wrapper::createFunction(
      library, name);
}

void MpsSlowOpsImpl::destroyFunction(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsFunction_t
        function) {
  ::orteaf::internal::runtime::mps::platform::wrapper::destroyFunction(
      function);
}

::orteaf::internal::runtime::mps::platform::wrapper::MpsComputePipelineState_t
MpsSlowOpsImpl::createComputePipelineState(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t device,
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsFunction_t
        function) {
  return ::orteaf::internal::runtime::mps::platform::wrapper::
      createComputePipelineState(device, function);
}

void MpsSlowOpsImpl::destroyComputePipelineState(
    ::orteaf::internal::runtime::mps::platform::wrapper::
        MpsComputePipelineState_t pipeline_state) {
  ::orteaf::internal::runtime::mps::platform::wrapper::
      destroyComputePipelineState(pipeline_state);
}

::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapDescriptor_t
MpsSlowOpsImpl::createHeapDescriptor() {
  return ::orteaf::internal::runtime::mps::platform::wrapper::
      createHeapDescriptor();
}

void MpsSlowOpsImpl::destroyHeapDescriptor(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapDescriptor_t
        descriptor) {
  ::orteaf::internal::runtime::mps::platform::wrapper::destroyHeapDescriptor(
      descriptor);
}

void MpsSlowOpsImpl::setHeapDescriptorSize(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapDescriptor_t
        descriptor,
    std::size_t size) {
  ::orteaf::internal::runtime::mps::platform::wrapper::setHeapDescriptorSize(
      descriptor, size);
}

void MpsSlowOpsImpl::setHeapDescriptorResourceOptions(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapDescriptor_t
        descriptor,
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsResourceOptions_t
        options) {
  ::orteaf::internal::runtime::mps::platform::wrapper::
      setHeapDescriptorResourceOptions(descriptor, options);
}

void MpsSlowOpsImpl::setHeapDescriptorStorageMode(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapDescriptor_t
        descriptor,
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsStorageMode_t
        storage_mode) {
  ::orteaf::internal::runtime::mps::platform::wrapper::
      setHeapDescriptorStorageMode(descriptor, storage_mode);
}

void MpsSlowOpsImpl::setHeapDescriptorCPUCacheMode(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapDescriptor_t
        descriptor,
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsCPUCacheMode_t
        cpu_cache_mode) {
  ::orteaf::internal::runtime::mps::platform::wrapper::
      setHeapDescriptorCPUCacheMode(descriptor, cpu_cache_mode);
}

void MpsSlowOpsImpl::setHeapDescriptorHazardTrackingMode(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapDescriptor_t
        descriptor,
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsHazardTrackingMode_t
        hazard_mode) {
  ::orteaf::internal::runtime::mps::platform::wrapper::
      setHeapDescriptorHazardTrackingMode(descriptor, hazard_mode);
}

void MpsSlowOpsImpl::setHeapDescriptorType(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapDescriptor_t
        descriptor,
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapType_t type) {
  ::orteaf::internal::runtime::mps::platform::wrapper::setHeapDescriptorType(
      descriptor, type);
}

::orteaf::internal::runtime::mps::platform::wrapper::MpsHeap_t
MpsSlowOpsImpl::createHeap(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t device,
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapDescriptor_t
        descriptor) {
  return ::orteaf::internal::runtime::mps::platform::wrapper::createHeap(
      device, descriptor);
}

void MpsSlowOpsImpl::destroyHeap(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeap_t heap) {
  ::orteaf::internal::runtime::mps::platform::wrapper::destroyHeap(heap);
}

::orteaf::internal::runtime::mps::platform::wrapper::MpsGraph_t
MpsSlowOpsImpl::createGraph() {
  return ::orteaf::internal::runtime::mps::platform::wrapper::createGraph();
}

void MpsSlowOpsImpl::destroyGraph(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraph_t graph) {
  ::orteaf::internal::runtime::mps::platform::wrapper::destroyGraph(graph);
}

::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphTensorData_t
MpsSlowOpsImpl::createGraphTensorData(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsBuffer_t buffer,
    const std::int64_t *shape, std::size_t shape_rank,
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphDataType
        data_type) {
  return ::orteaf::internal::runtime::mps::platform::wrapper::
      createGraphTensorDataFromBuffer(buffer, shape, shape_rank, data_type);
}

void MpsSlowOpsImpl::destroyGraphTensorData(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphTensorData_t
        tensor_data) {
  ::orteaf::internal::runtime::mps::platform::wrapper::destroyGraphTensorData(
      tensor_data);
}

::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphExecutable_t
MpsSlowOpsImpl::compileGraph(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraph_t graph,
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t device,
    const ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphFeed
        *feeds,
    std::size_t feed_count,
    const ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphTensor_t
        *target_tensors,
    std::size_t target_tensor_count,
    const ::orteaf::internal::runtime::mps::platform::wrapper::
        MpsGraphOperation_t *target_operations,
    std::size_t target_operation_count) {
  return ::orteaf::internal::runtime::mps::platform::wrapper::compileGraph(
      graph, device, feeds, feed_count, target_tensors, target_tensor_count,
      target_operations, target_operation_count);
}

std::size_t MpsSlowOpsImpl::runGraphExecutable(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphExecutable_t
        executable,
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsCommandQueue_t
        command_queue,
    const ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphFeed
        *feeds,
    std::size_t feed_count,
    const ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphTensor_t
        *target_tensors,
    std::size_t target_tensor_count,
    const ::orteaf::internal::runtime::mps::platform::wrapper::
        MpsGraphOperation_t *target_operations,
    std::size_t target_operation_count,
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphTensorData_t
        *out_tensor_data,
    std::size_t out_capacity) {
  return ::orteaf::internal::runtime::mps::platform::wrapper::
      runGraphExecutable(executable, command_queue, feeds, feed_count,
                         target_tensors, target_tensor_count, target_operations,
                         target_operation_count, out_tensor_data, out_capacity);
}

void MpsSlowOpsImpl::destroyGraphExecutable(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphExecutable_t
        executable) {
  ::orteaf::internal::runtime::mps::platform::wrapper::destroyGraphExecutable(
      executable);
}

} // namespace orteaf::internal::runtime::mps::platform

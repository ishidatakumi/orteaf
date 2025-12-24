#include "orteaf/internal/execution/mps/platform/mps_slow_ops.h"
#include "orteaf/internal/execution/mps/platform/wrapper/metal_kernel_embed_api.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_objc_bridge.h"

namespace orteaf::internal::execution::mps::platform {

int MpsSlowOpsImpl::getDeviceCount() {
  return ::orteaf::internal::execution::mps::platform::wrapper::getDeviceCount();
}

::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t
MpsSlowOpsImpl::getDevice(
    ::orteaf::internal::execution::mps::platform::wrapper::MPSInt_t index) {
  return ::orteaf::internal::execution::mps::platform::wrapper::getDevice(index);
}

void MpsSlowOpsImpl::releaseDevice(
    ::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t device) {
  ::orteaf::internal::execution::mps::platform::wrapper::deviceRelease(device);
}

::orteaf::internal::architecture::Architecture
MpsSlowOpsImpl::detectArchitecture(
    ::orteaf::internal::base::DeviceHandle device_id) {
  return ::orteaf::internal::architecture::detectMpsArchitectureForDeviceId(
      device_id);
}

::orteaf::internal::execution::mps::platform::wrapper::MpsCommandQueue_t
MpsSlowOpsImpl::createCommandQueue(
    ::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t device) {
  return ::orteaf::internal::execution::mps::platform::wrapper::
      createCommandQueue(device);
}

void MpsSlowOpsImpl::destroyCommandQueue(
    ::orteaf::internal::execution::mps::platform::wrapper::MpsCommandQueue_t
        queue) {
  ::orteaf::internal::execution::mps::platform::wrapper::destroyCommandQueue(
      queue);
}

::orteaf::internal::execution::mps::platform::wrapper::MpsEvent_t
MpsSlowOpsImpl::createEvent(
    ::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t device) {
  return ::orteaf::internal::execution::mps::platform::wrapper::createEvent(
      device);
}

void MpsSlowOpsImpl::destroyEvent(
    ::orteaf::internal::execution::mps::platform::wrapper::MpsEvent_t event) {
  ::orteaf::internal::execution::mps::platform::wrapper::destroyEvent(event);
}

::orteaf::internal::execution::mps::platform::wrapper::MpsFence_t
MpsSlowOpsImpl::createFence(
    ::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t device) {
  return ::orteaf::internal::execution::mps::platform::wrapper::createFence(
      device);
}

void MpsSlowOpsImpl::destroyFence(
    ::orteaf::internal::execution::mps::platform::wrapper::MpsFence_t fence) {
  ::orteaf::internal::execution::mps::platform::wrapper::destroyFence(fence);
}

::orteaf::internal::execution::mps::platform::wrapper::MpsLibrary_t
MpsSlowOpsImpl::createLibraryWithName(
    ::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t device,
    std::string_view name) {
  auto library = ::orteaf::internal::execution::mps::platform::
      metal_kernel_embed::createEmbeddedLibrary(device, name, nullptr);
  if (library != nullptr) {
    return library;
  }
  auto ns_name =
      ::orteaf::internal::execution::mps::platform::wrapper::toNsString(name);
  library = ::orteaf::internal::execution::mps::platform::wrapper::createLibrary(
      device, ns_name, nullptr);
  opaqueReleaseRetained(ns_name);
  return library;
}

void MpsSlowOpsImpl::destroyLibrary(
    ::orteaf::internal::execution::mps::platform::wrapper::MpsLibrary_t library) {
  ::orteaf::internal::execution::mps::platform::wrapper::destroyLibrary(library);
}

::orteaf::internal::execution::mps::platform::wrapper::MpsFunction_t
MpsSlowOpsImpl::createFunction(
    ::orteaf::internal::execution::mps::platform::wrapper::MpsLibrary_t library,
    std::string_view name) {
  return ::orteaf::internal::execution::mps::platform::wrapper::createFunction(
      library, name);
}

void MpsSlowOpsImpl::destroyFunction(
    ::orteaf::internal::execution::mps::platform::wrapper::MpsFunction_t
        function) {
  ::orteaf::internal::execution::mps::platform::wrapper::destroyFunction(
      function);
}

::orteaf::internal::execution::mps::platform::wrapper::MpsComputePipelineState_t
MpsSlowOpsImpl::createComputePipelineState(
    ::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t device,
    ::orteaf::internal::execution::mps::platform::wrapper::MpsFunction_t
        function) {
  return ::orteaf::internal::execution::mps::platform::wrapper::
      createComputePipelineState(device, function);
}

void MpsSlowOpsImpl::destroyComputePipelineState(
    ::orteaf::internal::execution::mps::platform::wrapper::
        MpsComputePipelineState_t pipeline_state) {
  ::orteaf::internal::execution::mps::platform::wrapper::
      destroyComputePipelineState(pipeline_state);
}

::orteaf::internal::execution::mps::platform::wrapper::MpsHeapDescriptor_t
MpsSlowOpsImpl::createHeapDescriptor() {
  return ::orteaf::internal::execution::mps::platform::wrapper::
      createHeapDescriptor();
}

void MpsSlowOpsImpl::destroyHeapDescriptor(
    ::orteaf::internal::execution::mps::platform::wrapper::MpsHeapDescriptor_t
        descriptor) {
  ::orteaf::internal::execution::mps::platform::wrapper::destroyHeapDescriptor(
      descriptor);
}

void MpsSlowOpsImpl::setHeapDescriptorSize(
    ::orteaf::internal::execution::mps::platform::wrapper::MpsHeapDescriptor_t
        descriptor,
    std::size_t size) {
  ::orteaf::internal::execution::mps::platform::wrapper::setHeapDescriptorSize(
      descriptor, size);
}

void MpsSlowOpsImpl::setHeapDescriptorResourceOptions(
    ::orteaf::internal::execution::mps::platform::wrapper::MpsHeapDescriptor_t
        descriptor,
    ::orteaf::internal::execution::mps::platform::wrapper::MpsResourceOptions_t
        options) {
  ::orteaf::internal::execution::mps::platform::wrapper::
      setHeapDescriptorResourceOptions(descriptor, options);
}

void MpsSlowOpsImpl::setHeapDescriptorStorageMode(
    ::orteaf::internal::execution::mps::platform::wrapper::MpsHeapDescriptor_t
        descriptor,
    ::orteaf::internal::execution::mps::platform::wrapper::MpsStorageMode_t
        storage_mode) {
  ::orteaf::internal::execution::mps::platform::wrapper::
      setHeapDescriptorStorageMode(descriptor, storage_mode);
}

void MpsSlowOpsImpl::setHeapDescriptorCPUCacheMode(
    ::orteaf::internal::execution::mps::platform::wrapper::MpsHeapDescriptor_t
        descriptor,
    ::orteaf::internal::execution::mps::platform::wrapper::MpsCPUCacheMode_t
        cpu_cache_mode) {
  ::orteaf::internal::execution::mps::platform::wrapper::
      setHeapDescriptorCPUCacheMode(descriptor, cpu_cache_mode);
}

void MpsSlowOpsImpl::setHeapDescriptorHazardTrackingMode(
    ::orteaf::internal::execution::mps::platform::wrapper::MpsHeapDescriptor_t
        descriptor,
    ::orteaf::internal::execution::mps::platform::wrapper::MpsHazardTrackingMode_t
        hazard_mode) {
  ::orteaf::internal::execution::mps::platform::wrapper::
      setHeapDescriptorHazardTrackingMode(descriptor, hazard_mode);
}

void MpsSlowOpsImpl::setHeapDescriptorType(
    ::orteaf::internal::execution::mps::platform::wrapper::MpsHeapDescriptor_t
        descriptor,
    ::orteaf::internal::execution::mps::platform::wrapper::MpsHeapType_t type) {
  ::orteaf::internal::execution::mps::platform::wrapper::setHeapDescriptorType(
      descriptor, type);
}

::orteaf::internal::execution::mps::platform::wrapper::MpsHeap_t
MpsSlowOpsImpl::createHeap(
    ::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t device,
    ::orteaf::internal::execution::mps::platform::wrapper::MpsHeapDescriptor_t
        descriptor) {
  return ::orteaf::internal::execution::mps::platform::wrapper::createHeap(
      device, descriptor);
}

void MpsSlowOpsImpl::destroyHeap(
    ::orteaf::internal::execution::mps::platform::wrapper::MpsHeap_t heap) {
  ::orteaf::internal::execution::mps::platform::wrapper::destroyHeap(heap);
}

::orteaf::internal::execution::mps::platform::wrapper::MpsGraph_t
MpsSlowOpsImpl::createGraph() {
  return ::orteaf::internal::execution::mps::platform::wrapper::createGraph();
}

void MpsSlowOpsImpl::destroyGraph(
    ::orteaf::internal::execution::mps::platform::wrapper::MpsGraph_t graph) {
  ::orteaf::internal::execution::mps::platform::wrapper::destroyGraph(graph);
}

::orteaf::internal::execution::mps::platform::wrapper::MpsGraphTensorData_t
MpsSlowOpsImpl::createGraphTensorData(
    ::orteaf::internal::execution::mps::platform::wrapper::MpsBuffer_t buffer,
    const std::int64_t *shape, std::size_t shape_rank,
    ::orteaf::internal::execution::mps::platform::wrapper::MpsGraphDataType
        data_type) {
  return ::orteaf::internal::execution::mps::platform::wrapper::
      createGraphTensorDataFromBuffer(buffer, shape, shape_rank, data_type);
}

void MpsSlowOpsImpl::destroyGraphTensorData(
    ::orteaf::internal::execution::mps::platform::wrapper::MpsGraphTensorData_t
        tensor_data) {
  ::orteaf::internal::execution::mps::platform::wrapper::destroyGraphTensorData(
      tensor_data);
}

::orteaf::internal::execution::mps::platform::wrapper::MpsGraphExecutable_t
MpsSlowOpsImpl::compileGraph(
    ::orteaf::internal::execution::mps::platform::wrapper::MpsGraph_t graph,
    ::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t device,
    const ::orteaf::internal::execution::mps::platform::wrapper::MpsGraphFeed
        *feeds,
    std::size_t feed_count,
    const ::orteaf::internal::execution::mps::platform::wrapper::MpsGraphTensor_t
        *target_tensors,
    std::size_t target_tensor_count,
    const ::orteaf::internal::execution::mps::platform::wrapper::
        MpsGraphOperation_t *target_operations,
    std::size_t target_operation_count) {
  return ::orteaf::internal::execution::mps::platform::wrapper::compileGraph(
      graph, device, feeds, feed_count, target_tensors, target_tensor_count,
      target_operations, target_operation_count);
}

std::size_t MpsSlowOpsImpl::runGraphExecutable(
    ::orteaf::internal::execution::mps::platform::wrapper::MpsGraphExecutable_t
        executable,
    ::orteaf::internal::execution::mps::platform::wrapper::MpsCommandQueue_t
        command_queue,
    const ::orteaf::internal::execution::mps::platform::wrapper::MpsGraphFeed
        *feeds,
    std::size_t feed_count,
    const ::orteaf::internal::execution::mps::platform::wrapper::MpsGraphTensor_t
        *target_tensors,
    std::size_t target_tensor_count,
    const ::orteaf::internal::execution::mps::platform::wrapper::
        MpsGraphOperation_t *target_operations,
    std::size_t target_operation_count,
    ::orteaf::internal::execution::mps::platform::wrapper::MpsGraphTensorData_t
        *out_tensor_data,
    std::size_t out_capacity) {
  return ::orteaf::internal::execution::mps::platform::wrapper::
      runGraphExecutable(executable, command_queue, feeds, feed_count,
                         target_tensors, target_tensor_count, target_operations,
                         target_operation_count, out_tensor_data, out_capacity);
}

void MpsSlowOpsImpl::destroyGraphExecutable(
    ::orteaf::internal::execution::mps::platform::wrapper::MpsGraphExecutable_t
        executable) {
  ::orteaf::internal::execution::mps::platform::wrapper::destroyGraphExecutable(
      executable);
}

} // namespace orteaf::internal::execution::mps::platform

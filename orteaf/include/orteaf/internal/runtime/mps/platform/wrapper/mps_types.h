#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>

#include "orteaf/internal/runtime/mps/platform/wrapper/mps_size.h"

namespace orteaf::internal::runtime::mps::platform::wrapper {

// Opaque pointer handles (PascalCase prefix for project types).
struct MpsBuffer_st;
using MpsBuffer_t = MpsBuffer_st *;
using MPSBuffer_t = MpsBuffer_t;

static_assert(sizeof(MpsBuffer_t) == sizeof(void *),
              "MpsBuffer must be pointer-sized.");
struct MpsCommandQueue_st;
using MpsCommandQueue_t = MpsCommandQueue_st *;
using MPSCommandQueue_t = MpsCommandQueue_t;

static_assert(sizeof(MpsCommandQueue_t) == sizeof(void *),
              "MpsCommandQueue must be pointer-sized.");
struct MpsCommandBuffer_st;
using MpsCommandBuffer_t = MpsCommandBuffer_st *;
using MPSCommandBuffer_t = MpsCommandBuffer_t;

static_assert(sizeof(MpsCommandBuffer_t) == sizeof(void *),
              "MpsCommandBuffer must be pointer-sized.");
struct MpsComputeCommandEncoder_st;
using MpsComputeCommandEncoder_t = MpsComputeCommandEncoder_st *;
using MPSComputeCommandEncoder_t = MpsComputeCommandEncoder_t;

static_assert(sizeof(MpsComputeCommandEncoder_t) == sizeof(void *),
              "MpsComputeCommandEncoder must be pointer-sized.");
struct MpsComputePipelineState_st;
using MpsComputePipelineState_t = MpsComputePipelineState_st *;
using MPSComputePipelineState_t = MpsComputePipelineState_t;

static_assert(sizeof(MpsComputePipelineState_t) == sizeof(void *),
              "MpsComputePipelineState must be pointer-sized.");
struct MpsFence_st;
using MpsFence_t = MpsFence_st *;
using MPSFence_t = MpsFence_t;

static_assert(sizeof(MpsFence_t) == sizeof(void *),
              "MpsFence must be pointer-sized.");
struct MpsEvent_st;
using MpsEvent_t = MpsEvent_st *;
using MPSEvent_t = MpsEvent_t;

static_assert(sizeof(MpsEvent_t) == sizeof(void *),
              "MpsEvent must be pointer-sized.");
struct MpsFunction_st;
using MpsFunction_t = MpsFunction_st *;
using MPSFunction_t = MpsFunction_t;

static_assert(sizeof(MpsFunction_t) == sizeof(void *),
              "MpsFunction must be pointer-sized.");
struct MpsLibrary_st;
using MpsLibrary_t = MpsLibrary_st *;
using MPSLibrary_t = MpsLibrary_t;

static_assert(sizeof(MpsLibrary_t) == sizeof(void *),
              "MpsLibrary must be pointer-sized.");
struct MpsDevice_st;
using MpsDevice_t = MpsDevice_st *;
using MPSDevice_t = MpsDevice_t;

static_assert(sizeof(MpsDevice_t) == sizeof(void *),
              "MpsDevice must be pointer-sized.");
struct MpsDeviceArray_st;
using MpsDeviceArray_t = MpsDeviceArray_st *;
using MPSDeviceArray_t = MpsDeviceArray_t;

static_assert(sizeof(MpsDeviceArray_t) == sizeof(void *),
              "MpsDeviceArray must be pointer-sized.");
struct MpsHeapDescriptor_st;
using MpsHeapDescriptor_t = MpsHeapDescriptor_st *;
using MPSHeapDescriptor_t = MpsHeapDescriptor_t;

static_assert(sizeof(MpsHeapDescriptor_t) == sizeof(void *),
              "MpsHeapDescriptor must be pointer-sized.");
struct MpsHeap_st;
using MpsHeap_t = MpsHeap_st *;
using MPSHeap_t = MpsHeap_t;

static_assert(sizeof(MpsHeap_t) == sizeof(void *),
              "MpsHeap must be pointer-sized.");
struct MpsCompileOptions_st;
using MpsCompileOptions_t = MpsCompileOptions_st *;
using MPSCompileOptions_t = MpsCompileOptions_t;

static_assert(sizeof(MpsCompileOptions_t) == sizeof(void *),
              "MpsCompileOptions must be pointer-sized.");
struct MpsTextureDescriptor_st;
using MpsTextureDescriptor_t = MpsTextureDescriptor_st *;
using MPSTextureDescriptor_t = MpsTextureDescriptor_t;

static_assert(sizeof(MpsTextureDescriptor_t) == sizeof(void *),
              "MpsTextureDescriptor must be pointer-sized.");
struct MpsTexture_st;
using MpsTexture_t = MpsTexture_st *;
using MPSTexture_t = MpsTexture_t;

static_assert(sizeof(MpsTexture_t) == sizeof(void *),
              "MpsTexture must be pointer-sized.");
struct MpsGraph_st;
using MpsGraph_t = MpsGraph_st *;
using MPSGraph_t = MpsGraph_t;

static_assert(sizeof(MpsGraph_t) == sizeof(void *),
              "MpsGraph must be pointer-sized.");
struct MpsGraphTensor_st;
using MpsGraphTensor_t = MpsGraphTensor_st *;
using MPSGraphTensor_t = MpsGraphTensor_t;

static_assert(sizeof(MpsGraphTensor_t) == sizeof(void *),
              "MpsGraphTensor must be pointer-sized.");
struct MpsGraphOperation_st;
using MpsGraphOperation_t = MpsGraphOperation_st *;
using MPSGraphOperation_t = MpsGraphOperation_t;

static_assert(sizeof(MpsGraphOperation_t) == sizeof(void *),
              "MpsGraphOperation must be pointer-sized.");
struct MpsGraphExecutable_st;
using MpsGraphExecutable_t = MpsGraphExecutable_st *;
using MPSGraphExecutable_t = MpsGraphExecutable_t;

static_assert(sizeof(MpsGraphExecutable_t) == sizeof(void *),
              "MpsGraphExecutable must be pointer-sized.");
struct MpsGraphTensorData_st;
using MpsGraphTensorData_t = MpsGraphTensorData_st *;
using MPSGraphTensorData_t = MpsGraphTensorData_t;

static_assert(sizeof(MpsGraphTensorData_t) == sizeof(void *),
              "MpsGraphTensorData must be pointer-sized.");
struct MpsString_st;
using MpsString_t = MpsString_st *;
using MPSString_t = MpsString_t;

static_assert(sizeof(MpsString_t) == sizeof(void *),
              "MpsString must be pointer-sized.");
struct MpsError_st;
using MpsError_t = MpsError_st *;
using MPSError_t = MpsError_t;

static_assert(sizeof(MpsError_t) == sizeof(void *),
              "MpsError must be pointer-sized.");
// Scalar/enum aliases.
using MpsBufferUsage_t = unsigned long;
using MPSBufferUsage_t = MpsBufferUsage_t;

using MpsStorageMode_t = std::uint32_t;
using MPSStorageMode_t = MpsStorageMode_t;

using MpsCPUCacheMode_t = std::uint32_t;
using MPSCPUCacheMode_t = MpsCPUCacheMode_t;

using MpsHazardTrackingMode_t = std::uint32_t;
using MPSHazardTrackingMode_t = MpsHazardTrackingMode_t;

using MpsHeapType_t = std::uint32_t;
using MPSHeapType_t = MpsHeapType_t;

using MpsResourceOptions_t = std::uint64_t;
using MPSResourceOptions_t = MpsResourceOptions_t;

using MpsTextureType_t = std::uint32_t;
using MPSTextureType_t = MpsTextureType_t;

using MpsPixelFormat_t = std::uint32_t;
using MPSPixelFormat_t = MpsPixelFormat_t;

} // namespace orteaf::internal::runtime::mps::platform::wrapper

#endif // ORTEAF_ENABLE_MPS

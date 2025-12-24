#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>

#include "orteaf/internal/execution/mps/platform/wrapper/mps_size.h"

namespace orteaf::internal::execution::mps::platform::wrapper {

// Opaque pointer handles (PascalCase prefix for project types).
struct MpsBuffer_st;
using MpsBuffer_t = MpsBuffer_st *;

static_assert(sizeof(MpsBuffer_t) == sizeof(void *),
              "MpsBuffer must be pointer-sized.");
struct MpsCommandQueue_st;
using MpsCommandQueue_t = MpsCommandQueue_st *;

static_assert(sizeof(MpsCommandQueue_t) == sizeof(void *),
              "MpsCommandQueue must be pointer-sized.");
struct MpsCommandBuffer_st;
using MpsCommandBuffer_t = MpsCommandBuffer_st *;

static_assert(sizeof(MpsCommandBuffer_t) == sizeof(void *),
              "MpsCommandBuffer must be pointer-sized.");
struct MpsComputeCommandEncoder_st;
using MpsComputeCommandEncoder_t = MpsComputeCommandEncoder_st *;

static_assert(sizeof(MpsComputeCommandEncoder_t) == sizeof(void *),
              "MpsComputeCommandEncoder must be pointer-sized.");
struct MpsComputePipelineState_st;
using MpsComputePipelineState_t = MpsComputePipelineState_st *;

static_assert(sizeof(MpsComputePipelineState_t) == sizeof(void *),
              "MpsComputePipelineState must be pointer-sized.");
struct MpsFence_st;
using MpsFence_t = MpsFence_st *;

static_assert(sizeof(MpsFence_t) == sizeof(void *),
              "MpsFence must be pointer-sized.");
struct MpsEvent_st;
using MpsEvent_t = MpsEvent_st *;

static_assert(sizeof(MpsEvent_t) == sizeof(void *),
              "MpsEvent must be pointer-sized.");
struct MpsFunction_st;
using MpsFunction_t = MpsFunction_st *;

static_assert(sizeof(MpsFunction_t) == sizeof(void *),
              "MpsFunction must be pointer-sized.");
struct MpsLibrary_st;
using MpsLibrary_t = MpsLibrary_st *;

static_assert(sizeof(MpsLibrary_t) == sizeof(void *),
              "MpsLibrary must be pointer-sized.");
struct MpsDevice_st;
using MpsDevice_t = MpsDevice_st *;

static_assert(sizeof(MpsDevice_t) == sizeof(void *),
              "MpsDevice must be pointer-sized.");
struct MpsDeviceArray_st;
using MpsDeviceArray_t = MpsDeviceArray_st *;

static_assert(sizeof(MpsDeviceArray_t) == sizeof(void *),
              "MpsDeviceArray must be pointer-sized.");
struct MpsHeapDescriptor_st;
using MpsHeapDescriptor_t = MpsHeapDescriptor_st *;

static_assert(sizeof(MpsHeapDescriptor_t) == sizeof(void *),
              "MpsHeapDescriptor must be pointer-sized.");
struct MpsHeap_st;
using MpsHeap_t = MpsHeap_st *;

static_assert(sizeof(MpsHeap_t) == sizeof(void *),
              "MpsHeap must be pointer-sized.");
struct MpsCompileOptions_st;
using MpsCompileOptions_t = MpsCompileOptions_st *;

static_assert(sizeof(MpsCompileOptions_t) == sizeof(void *),
              "MpsCompileOptions must be pointer-sized.");
struct MpsTextureDescriptor_st;
using MpsTextureDescriptor_t = MpsTextureDescriptor_st *;

static_assert(sizeof(MpsTextureDescriptor_t) == sizeof(void *),
              "MpsTextureDescriptor must be pointer-sized.");
struct MpsTexture_st;
using MpsTexture_t = MpsTexture_st *;

static_assert(sizeof(MpsTexture_t) == sizeof(void *),
              "MpsTexture must be pointer-sized.");
struct MpsGraph_st;
using MpsGraph_t = MpsGraph_st *;

static_assert(sizeof(MpsGraph_t) == sizeof(void *),
              "MpsGraph must be pointer-sized.");
struct MpsGraphTensor_st;
using MpsGraphTensor_t = MpsGraphTensor_st *;

static_assert(sizeof(MpsGraphTensor_t) == sizeof(void *),
              "MpsGraphTensor must be pointer-sized.");
struct MpsGraphOperation_st;
using MpsGraphOperation_t = MpsGraphOperation_st *;

static_assert(sizeof(MpsGraphOperation_t) == sizeof(void *),
              "MpsGraphOperation must be pointer-sized.");
struct MpsGraphExecutable_st;
using MpsGraphExecutable_t = MpsGraphExecutable_st *;

static_assert(sizeof(MpsGraphExecutable_t) == sizeof(void *),
              "MpsGraphExecutable must be pointer-sized.");
struct MpsGraphTensorData_st;
using MpsGraphTensorData_t = MpsGraphTensorData_st *;

static_assert(sizeof(MpsGraphTensorData_t) == sizeof(void *),
              "MpsGraphTensorData must be pointer-sized.");
struct MpsString_st;
using MpsString_t = MpsString_st *;

static_assert(sizeof(MpsString_t) == sizeof(void *),
              "MpsString must be pointer-sized.");
struct MpsError_st;
using MpsError_t = MpsError_st *;

static_assert(sizeof(MpsError_t) == sizeof(void *),
              "MpsError must be pointer-sized.");
// Scalar/enum aliases.
using MpsBufferUsage_t = unsigned long;

using MpsStorageMode_t = std::uint32_t;

using MpsCPUCacheMode_t = std::uint32_t;

using MpsHazardTrackingMode_t = std::uint32_t;

using MpsHeapType_t = std::uint32_t;

using MpsResourceOptions_t = std::uint64_t;

using MpsTextureType_t = std::uint32_t;

using MpsPixelFormat_t = std::uint32_t;

} // namespace orteaf::internal::execution::mps::platform::wrapper

#endif // ORTEAF_ENABLE_MPS

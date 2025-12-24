/**
 * @file cuda_dim.cu
 * @brief Implementation for CUDA dimension helper PODs and converters.
 */
#ifndef __CUDACC__
#error "cuda_dim.cu must be compiled with a CUDA compiler (__CUDACC__ not defined)"
#endif
#include "orteaf/internal/execution/cuda/platform/wrapper/cuda_dim.h"

namespace orteaf::internal::execution::cuda::platform::wrapper {

/**
 * @copydoc orteaf::internal::backend::cuda::makeDim3
 */
CudaDim3_t makeDim3(std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept {
    return CudaDim3_t{x, y, z};
}

/**
 * @copydoc orteaf::internal::backend::cuda::makeUInt3
 */
CudaUInt3_t makeUInt3(std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept {
    return CudaUInt3_t{x, y, z};
}

/** Convert `CudaDim3_t` to CUDA `dim3`. */
dim3 toCudaDim3(CudaDim3_t value) noexcept {
    dim3 result{};
    result.x = value.x;
    result.y = value.y;
    result.z = value.z;
    return result;
}

/** Convert CUDA `dim3` to `CudaDim3_t`. */
CudaDim3_t fromCudaDim3(dim3 value) noexcept {
    return CudaDim3_t{value.x, value.y, value.z};
}

/** Convert `CudaUInt3_t` to CUDA `uint3`. */
uint3 toCudaUInt3(CudaUInt3_t value) noexcept {
    uint3 result{};
    result.x = value.x;
    result.y = value.y;
    result.z = value.z;
    return result;
}

/** Convert CUDA `uint3` to `CudaUInt3_t`. */
CudaUInt3_t fromCudaUInt3(uint3 value) noexcept {
    return CudaUInt3_t{value.x, value.y, value.z};
}

} // namespace orteaf::internal::execution::cuda::platform::wrapper

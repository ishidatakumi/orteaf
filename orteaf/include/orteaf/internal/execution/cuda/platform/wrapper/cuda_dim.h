/**
 * @file cuda_dim.h
 * @brief Lightweight POD types and helpers for CUDA 3D dimensions.
 *
 * Defines `CudaDim3_t` and `CudaUInt3_t` as plain 3x32-bit structs that mirror
 * CUDA's `dim3` and `uint3` layout/ABI. Conversion helpers are provided when
 * CUDA is enabled in headers that include `<vector_types.h>`; otherwise size
 * checks verify ABI assumptions.
 */
#pragma once

#include <cstdint>

#if ORTEAF_ENABLE_CUDA
#include <vector_types.h>

namespace orteaf::internal::execution::cuda::platform::wrapper {

/**
 * @brief 3D grid/block shape (unsigned 32-bit components).
 */
struct CudaDim3_st {
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t z;
};

using CudaDim3_t = CudaDim3_st;

/**
 * @brief Generic 3D unsigned vector (uint3 equivalent).
 */
struct CudaUInt3_st {
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t z;
};

using CudaUInt3_t = CudaUInt3_st;

static_assert(sizeof(CudaDim3_t) == 3 * sizeof(std::uint32_t), "CudaDim3_t must pack three 32-bit integers.");
static_assert(sizeof(CudaUInt3_t) == 3 * sizeof(std::uint32_t), "CudaUInt3_t must pack three 32-bit integers.");

static_assert(sizeof(CudaDim3_t) == sizeof(dim3), "dim3 has unexpected size.");
static_assert(sizeof(CudaUInt3_t) == sizeof(uint3), "uint3 has unexpected size.");

/**
 * @brief Construct a `CudaDim3_t` from components.
 */
CudaDim3_t makeDim3(std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept;
/**
 * @brief Construct a `CudaUInt3_t` from components.
 */
CudaUInt3_t makeUInt3(std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept;

#if defined(__CUDACC__)
/** Convert `CudaDim3_t` to CUDA `dim3`. */
dim3 toCudaDim3(CudaDim3_t value) noexcept;
/** Convert CUDA `dim3` to `CudaDim3_t`. */
CudaDim3_t fromCudaDim3(dim3 value) noexcept;

/** Convert `CudaUInt3_t` to CUDA `uint3`. */
uint3 toCudaUInt3(CudaUInt3_t value) noexcept;
/** Convert CUDA `uint3` to `CudaUInt3_t`. */
CudaUInt3_t fromCudaUInt3(uint3 value) noexcept;
#endif  // defined(__CUDACC__)
} // namespace orteaf::internal::execution::cuda::platform::wrapper

#endif  // ORTEAF_ENABLE_CUDA
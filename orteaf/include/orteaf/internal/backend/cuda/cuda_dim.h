#pragma once

#include <cstdint>

#if defined(CUDA_AVAILABLE)
#include <vector_types.h>
#endif

struct CudaDim3_st {
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t z;
};

using CudaDim3_t = CudaDim3_st;

struct CudaUInt3_st {
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t z;
};

using CudaUInt3_t = CudaUInt3_st;

static_assert(sizeof(CudaDim3_t) == 3 * sizeof(std::uint32_t), "CudaDim3_t must pack three 32-bit integers.");
static_assert(sizeof(CudaUInt3_t) == 3 * sizeof(std::uint32_t), "CudaUInt3_t must pack three 32-bit integers.");

#if defined(CUDA_AVAILABLE)
static_assert(sizeof(CudaDim3_t) == sizeof(dim3), "dim3 has unexpected size.");
static_assert(sizeof(CudaUInt3_t) == sizeof(uint3), "uint3 has unexpected size.");
#endif

namespace orteaf::internal::backend::cuda {

CudaDim3_t make_dim3(std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept;
CudaUInt3_t make_uint3(std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept;

#if defined(CUDA_AVAILABLE)
dim3 to_cuda_dim3(CudaDim3_t value) noexcept;
CudaDim3_t from_cuda_dim3(dim3 value) noexcept;

uint3 to_cuda_uint3(CudaUInt3_t value) noexcept;
CudaUInt3_t from_cuda_uint3(uint3 value) noexcept;
#endif

} // namespace orteaf::internal::backend::cuda

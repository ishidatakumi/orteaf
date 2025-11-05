
#pragma once
#include <cstdint>
#include "orteaf/internal/backend/cuda/cuda_device.h"

struct CUstream_st;
using CUstream_t = CUstream_st*;
using CUdeviceptr_t = std::uint64_t;

static_assert(sizeof(CUdeviceptr_t) == sizeof(std::uint64_t), "CUdeviceptr_t must match 64-bit width.");

// ABI guard: must be pointer-sized on every platform
static_assert(sizeof(CUstream_t) == sizeof(void*), "CUstream_t must be pointer-sized.");

namespace orteaf::internal::backend::cuda {

CUstream_t get_stream();
void set_stream(CUstream_t stream);
void release_stream(CUstream_t stream);
void synchronize_stream(CUstream_t stream);
void wait_stream(CUstream_t stream, CUdeviceptr_t addr, uint32_t value);
void write_stream(CUstream_t stream, CUdeviceptr_t addr, uint32_t value);
} // namespace orteaf::internal::backend::cuda

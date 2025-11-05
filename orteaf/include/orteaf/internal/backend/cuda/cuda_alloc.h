#pragma once

#include <cstddef>
#include "orteaf/internal/backend/cuda/cuda_stream.h"

namespace orteaf::internal::backend::cuda {
CUdeviceptr_t alloc(size_t size);
void free(CUdeviceptr_t ptr, size_t size);
// stream
CUdeviceptr_t alloc_stream(size_t size, CUstream_t stream);
void free_stream(CUdeviceptr_t ptr, size_t size, CUstream_t stream);
// host
void* alloc_host(size_t size);
void copy_to_host(CUdeviceptr_t ptr, void* host_ptr, size_t size);
void copy_to_device(void* host_ptr, CUdeviceptr_t ptr, size_t size);
void free_host(void* ptr, size_t size);

} // namespace orteaf::internal::backend::cuda

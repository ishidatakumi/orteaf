#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>

#include <orteaf/internal/backend/backend.h>
#include <orteaf/internal/backend/cpu/cpu_buffer_handle.h>
#include <orteaf/internal/backend/cuda/cuda_device.h>
#if ORTEAF_ENABLE_CUDA
#include <orteaf/internal/backend/cuda/cuda_stream.h>
#include <orteaf/internal/backend/cuda/cuda_buffer_handle.h>
#endif

#if ORTEAF_ENABLE_MPS
#include <orteaf/internal/backend/mps/mps_buffer_handle.h>
#include <orteaf/internal/backend/mps/mps_command_queue.h>
#include <orteaf/internal/backend/mps/mps_device.h>
#endif

namespace orteaf::internal::backend {

template <Backend B>
struct BackendTraits;

// CPU
template <>
struct BackendTraits<Backend::CPU> {
    using BufferHandle = cpu::CpuBufferHandle;
    using Stream = void*;      // placeholder; adjust when stream type is defined
    using Device = int;        // placeholder; adjust when device abstraction is defined
};

// CUDA
#if ORTEAF_ENABLE_CUDA
template <>
struct BackendTraits<Backend::CUDA> {
    using BufferHandle = cuda::CudaBufferHandle;
    using Stream = cuda::CUstream_t;     // CUDA stream handle
    using Device = cuda::CUdevice_t;     // opaque CUDA device handle
};
#endif  // ORTEAF_ENABLE_CUDA

// MPS
#if ORTEAF_ENABLE_MPS
template <>
struct BackendTraits<Backend::MPS> {
    using BufferHandle = mps::MpsBufferHandle;
    using Stream = MPSCommandQueue_t;    // command queue as stream token
    using Device = mps::MPSDevice_t;     // opaque Metal device handle
};
#endif  // ORTEAF_ENABLE_MPS

}  // namespace orteaf::internal::backend

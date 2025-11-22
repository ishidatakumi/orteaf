#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>

#include <orteaf/internal/backend/backend.h>
#include <orteaf/internal/backend/cpu/cpu_buffer_view.h>
#if ORTEAF_ENABLE_CUDA
#include <orteaf/internal/backend/cuda/wrapper/cuda_device.h>
#include <orteaf/internal/backend/cuda/wrapper/cuda_stream.h>
#include <orteaf/internal/backend/cuda/cuda_buffer_view.h>
#endif

#if ORTEAF_ENABLE_MPS
#include <orteaf/internal/backend/mps/mps_buffer_view.h>
#include <orteaf/internal/backend/mps/wrapper/mps_command_queue.h>
#include <orteaf/internal/backend/mps/wrapper/mps_device.h>
#endif

namespace orteaf::internal::backend {

template <Backend B>
struct BackendTraits;

// CPU
template <>
struct BackendTraits<Backend::CPU> {
    using BufferView = cpu::CpuBufferView;
    using Stream = void*;      // placeholder; adjust when stream type is defined
    using Device = int;        // placeholder; adjust when device abstraction is defined
};

// CUDA
#if ORTEAF_ENABLE_CUDA
template <>
struct BackendTraits<Backend::CUDA> {
    using BufferView = cuda::CudaBufferView;
    using Stream = cuda::CUstream_t;     // CUDA stream handle
    using Device = cuda::CUdevice_t;     // opaque CUDA device handle
};
#endif  // ORTEAF_ENABLE_CUDA

// MPS
#if ORTEAF_ENABLE_MPS
template <>
struct BackendTraits<Backend::MPS> {
    using BufferView = mps::MpsBufferView;
    using Stream = MPSCommandQueue_t;    // command queue as stream token
    using Device = mps::MPSDevice_t;     // opaque Metal device handle
};
#endif  // ORTEAF_ENABLE_MPS

}  // namespace orteaf::internal::backend

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>

#include <orteaf/internal/backend/backend.h>
#include <orteaf/internal/runtime/cpu/resource/cpu_buffer_view.h>
#include <orteaf/internal/runtime/cpu/resource/cpu_heap_region.h>
#include <orteaf/internal/runtime/cpu/resource/cpu_tokens.h>
#if ORTEAF_ENABLE_CUDA
#include <orteaf/internal/runtime/cuda/platform/wrapper/cuda_device.h>
#include <orteaf/internal/runtime/cuda/platform/wrapper/cuda_stream.h>
#include <orteaf/internal/runtime/cuda/resource/cuda_buffer_view.h>
#include <orteaf/internal/runtime/cuda/resource/cuda_tokens.h>
#endif

#if ORTEAF_ENABLE_MPS
#include <orteaf/internal/runtime/mps/platform/wrapper/mps_command_queue.h>
#include <orteaf/internal/runtime/mps/platform/wrapper/mps_device.h>
#include <orteaf/internal/runtime/mps/resource/mps_buffer_view.h>
#include <orteaf/internal/runtime/mps/resource/mps_fence_token.h>
#include <orteaf/internal/runtime/mps/resource/mps_heap_region.h>
#include <orteaf/internal/runtime/mps/resource/mps_reuse_token.h>
#include <orteaf/internal/runtime/mps/manager/mps_command_queue_manager.h>
#include <orteaf/internal/runtime/mps/manager/mps_device_manager.h>
#endif

namespace orteaf::internal::runtime::base {

template <::orteaf::internal::backend::Backend B> struct BackendTraits;

// CPU
template <> struct BackendTraits<::orteaf::internal::backend::Backend::Cpu> {
  using BufferView = ::orteaf::internal::runtime::cpu::resource::CpuBufferView;
  using HeapRegion = ::orteaf::internal::runtime::cpu::resource::CpuHeapRegion;
  using Stream = void *; // placeholder; adjust when stream type is defined
  using Device = int; // placeholder; adjust when device abstraction is defined
  using Context =
      int; // placeholder; adjust when context abstraction is defined
  using FenceToken = ::orteaf::internal::runtime::cpu::resource::FenceToken;
  using ReuseToken = ::orteaf::internal::runtime::cpu::resource::ReuseToken;
  struct KernelLaunchParams {
    Stream stream{nullptr};
    Device device{};
  };
};

// CUDA
#if ORTEAF_ENABLE_CUDA
template <> struct BackendTraits<::orteaf::internal::backend::Backend::Cuda> {
  using BufferView =
      ::orteaf::internal::runtime::cuda::resource::CudaBufferView;
  using HeapRegion = ::orteaf::internal::runtime::cuda::resource::
      CudaBufferView; // TODO: replace with dedicated region type
  using Stream = ::orteaf::internal::runtime::cuda::platform::wrapper::
      CudaStream_t; // CUDA stream handle
  using Device = ::orteaf::internal::runtime::cuda::platform::wrapper::
      CudaDevice_t;      // opaque CUDA device handle
  using Context = int; // placeholder until context abstraction exists
  using FenceToken = ::orteaf::internal::runtime::cuda::resource::
      FenceToken; // placeholder fence token until CUDA token is defined
  using ReuseToken = ::orteaf::internal::runtime::cuda::resource::
      ReuseToken; // placeholder reuse token until CUDA token is defined
  struct KernelLaunchParams {
    Device device{};
    Stream stream{};
  };
};
#endif // ORTEAF_ENABLE_CUDA

// MPS
#if ORTEAF_ENABLE_MPS
template <> struct BackendTraits<::orteaf::internal::backend::Backend::Mps> {
  using BufferView = ::orteaf::internal::runtime::mps::resource::MpsBufferView;
  using HeapRegion = ::orteaf::internal::runtime::mps::resource::MpsHeapRegion;
  using Context = int; // placeholder until context abstraction exists
  using Stream = ::orteaf::internal::runtime::mps::platform::wrapper::
      MPSCommandQueue_t; // command queue as stream token
  using Device = ::orteaf::internal::runtime::mps::platform::wrapper::
      MPSDevice_t; // opaque Metal device handle
  using FenceToken = ::orteaf::internal::runtime::mps::resource::MpsFenceToken;
  using ReuseToken = ::orteaf::internal::runtime::mps::resource::MpsReuseToken;
  struct KernelLaunchParams {
    using DeviceLease =
        ::orteaf::internal::runtime::mps::manager::MpsDeviceManager::
            DeviceLease;
    using CommandQueueLease =
        ::orteaf::internal::runtime::mps::manager::MpsCommandQueueManager::
            CommandQueueLease;
    DeviceLease device{};                // optional; when empty falls back to resource device
    CommandQueueLease command_queue{};   // optional; when empty a new queue is created
  };
};
#endif // ORTEAF_ENABLE_MPS

} // namespace orteaf::internal::runtime::base

#pragma once

#include <utility>

#include <orteaf/internal/backend/backend.h>
#include <orteaf/internal/base/handle.h>
#include <orteaf/internal/execution/cpu/resource/cpu_buffer_view.h>

#if ORTEAF_ENABLE_CUDA
#include <orteaf/internal/execution/cuda/resource/cuda_buffer_view.h>
#endif // ORTEAF_ENABLE_CUDA

#if ORTEAF_ENABLE_MPS
#include <orteaf/internal/execution/mps/resource/mps_buffer_view.h>
#include <orteaf/internal/execution/mps/resource/mps_fence_token.h>
#endif // ORTEAF_ENABLE_MPS

namespace orteaf::internal::execution::allocator {
struct CpuFenceToken {};

template <backend::Backend B> struct ResourceBufferType {
  using view = ::orteaf::internal::execution::cpu::resource::CpuBufferView;
  using fence_token = CpuFenceToken;
};

#if ORTEAF_ENABLE_CUDA
template <> struct ResourceBufferType<backend::Backend::Cuda> {
  using view = ::orteaf::internal::execution::cuda::resource::CudaBufferView;
};
#endif // ORTEAF_ENABLE_CUDA

#if ORTEAF_ENABLE_MPS
template <> struct ResourceBufferType<backend::Backend::Mps> {
  using view = ::orteaf::internal::execution::mps::resource::MpsBufferView;
  using fence_token = ::orteaf::internal::execution::mps::resource::MpsFenceToken;
};
#endif // ORTEAF_ENABLE_MPS

// Lightweight pair of buffer view and handle (no fence tracking).
template <backend::Backend B> struct BufferBlock {
  using BufferView = typename ResourceBufferType<B>::view;
  using BufferViewHandle = ::orteaf::internal::base::BufferViewHandle;

  BufferViewHandle handle{};
  BufferView view{};

  BufferBlock() = default;
  BufferBlock(BufferViewHandle h, BufferView v)
      : handle(h), view(std::move(v)) {}

  bool valid() const { return handle.isValid() && static_cast<bool>(view); }
};

// Non-owning view of a buffer with an associated strong ID.
template <backend::Backend B> struct BufferResource {
  using BufferView = typename ResourceBufferType<B>::view;
  using BufferViewHandle = ::orteaf::internal::base::BufferViewHandle;
  using FenceToken = typename ResourceBufferType<B>::fence_token;

  BufferViewHandle handle{};
  BufferView view{};
  FenceToken fence_token{};

  BufferResource() = default;
  BufferResource(BufferViewHandle handle, BufferView view)
      : handle(handle), view(std::move(view)) {}

  // Convert to BufferBlock (discards fence_token)
  BufferBlock<B> toBlock() const { return BufferBlock<B>{handle, view}; }

  // Construct from BufferBlock (fence_token is default-initialized)
  static BufferResource fromBlock(const BufferBlock<B> &block) {
    return BufferResource{block.handle, block.view};
  }

  bool valid() const { return handle.isValid() && static_cast<bool>(view); }
};

} // namespace orteaf::internal::execution::allocator

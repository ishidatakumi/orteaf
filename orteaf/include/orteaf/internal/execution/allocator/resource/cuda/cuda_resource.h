#pragma once

#if ORTEAF_ENABLE_CUDA

#include <cstddef>

#include "orteaf/internal/execution/cuda/resource/cuda_buffer_view.h"
#include "orteaf/internal/execution/cuda/resource/cuda_tokens.h"

namespace orteaf::internal::execution::cuda::resource {

// Simple CUDA resource that directly allocates/free device buffers.
class CudaResource {
public:
  using BufferView = ::orteaf::internal::execution::cuda::resource::CudaBufferView;
  using FenceToken = ::orteaf::internal::execution::cuda::resource::FenceToken;
  using ReuseToken = ::orteaf::internal::execution::cuda::resource::ReuseToken;

  struct Config {};

  static void initialize(const Config &config = {}) noexcept;

  static BufferView allocate(std::size_t size, std::size_t alignment);

  static void deallocate(BufferView view, std::size_t size,
                         std::size_t alignment);

  static bool isCompleted(const FenceToken &token);
  static bool isCompleted(const ReuseToken &token);

  static BufferView makeView(BufferView base, std::size_t offset,
                             std::size_t size);
};

} // namespace orteaf::internal::execution::cuda::resource

#endif // ORTEAF_ENABLE_CUDA

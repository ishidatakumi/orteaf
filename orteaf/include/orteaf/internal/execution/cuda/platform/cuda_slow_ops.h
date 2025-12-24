#pragma once

#if ORTEAF_ENABLE_CUDA

namespace orteaf::internal::execution::cuda::platform {

// CUDA slow-path operations interface. Virtual to allow mocking and late binding.
struct CudaSlowOps {
  virtual ~CudaSlowOps() = default;
};

// Default implementation placeholder.
struct CudaSlowOpsImpl final : public CudaSlowOps {};

} // namespace orteaf::internal::execution::cuda::platform

#endif // ORTEAF_ENABLE_CUDA


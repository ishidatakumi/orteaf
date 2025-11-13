/**
 * @file cuda_init.cu
 * @brief Implementation of process-wide CUDA Driver initialization.
 */
#include "orteaf/internal/backend/cuda/cuda_init.h"

#ifdef ORTEAF_ENABLE_CUDA

#include <mutex>
#include <cuda.h>
#include "orteaf/internal/backend/cuda/cuda_check.h"

namespace orteaf::internal::backend::cuda {

namespace {
std::once_flag g_cuda_driver_init_flag;
}

/**
 * @copydoc orteaf::internal::backend::cuda::cudaInit
 */
void cudaInit() {
    std::call_once(g_cuda_driver_init_flag, []() {
        CU_CHECK(cuInit(0));
    });
}

} // namespace orteaf::internal::backend::cuda

#else  // !ORTEAF_ENABLE_CUDA

#include "orteaf/internal/diagnostics/error/error_impl.h"

namespace orteaf::internal::backend::cuda {

/**
 * @copydoc orteaf::internal::backend::cuda::cudaInit
 */
void cudaInit() {
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "cudaInit: CUDA backend is not available (CUDA disabled)");
}

} // namespace orteaf::internal::backend::cuda

#endif

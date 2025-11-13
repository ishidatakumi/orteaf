/**
 * @file cuda_init.cu
 * @brief Implementation of process-wide CUDA Driver initialization.
 */
#ifndef __CUDACC__
#error "cuda_init.cu must be compiled with a CUDA compiler (__CUDACC__ not defined)"
#endif
#include "orteaf/internal/backend/cuda/cuda_init.h"

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

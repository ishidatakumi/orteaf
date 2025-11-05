#include "orteaf/internal/backend/cuda/cuda_init.h"

#ifdef CUDA_AVAILABLE

#include <mutex>
#include <cuda.h>
#include "orteaf/internal/backend/cuda/cuda_check.h"

namespace orteaf::internal::backend::cuda {

namespace {
std::once_flag g_cuda_driver_init_flag;
}

void cuda_init() {
    std::call_once(g_cuda_driver_init_flag, []() {
        CU_CHECK(cuInit(0));
    });
}

} // namespace orteaf::internal::backend::cuda

#endif

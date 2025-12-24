/**
 * @file cuda_context.cu
 * @brief Implementation of CUDA context helpers (retain/create/set/release).
 *
 * These functions wrap CUDA Driver API calls and surface errors through
 * `OrteafErrc` as `std::system_error` via `CU_CHECK`. When CUDA is disabled,
 * implementations are effectively no-ops and return nullptr where applicable.
 */
#ifndef __CUDACC__
#error "cuda_context.cu must be compiled with a CUDA compiler (__CUDACC__ not defined)"
#endif
#include "orteaf/internal/execution/cuda/platform/wrapper/cuda_context.h"
#include "orteaf/internal/execution/cuda/platform/wrapper/cuda_device.h"
#include "orteaf/internal/execution/cuda/platform/wrapper/cuda_objc_bridge.h"

#include "orteaf/internal/diagnostics/error/error.h"
#include <cuda.h>
#include "orteaf/internal/execution/cuda/platform/wrapper/cuda_check.h"

namespace orteaf::internal::execution::cuda::platform::wrapper {

/**
 * @copydoc orteaf::internal::backend::cuda::getPrimaryContext
 */
CudaContext_t getPrimaryContext(CudaDevice_t device) {
    CUdevice objc_device = cuDeviceFromOpaque(device);
    CUcontext context = nullptr;
    CU_CHECK(cuDevicePrimaryCtxRetain(&context, objc_device));
    return opaqueFromObjcNoown<CudaContext_t, CUcontext>(context);
}

/**
 * @copydoc orteaf::internal::backend::cuda::createContext
 */
CudaContext_t createContext(CudaDevice_t device) {
    CUdevice objc_device = cuDeviceFromOpaque(device);
    CUcontext context = nullptr;
#if defined(CUDA_VERSION) && CUDA_VERSION >= 13000
    // CUDA 13 switched cuCtxCreate -> cuCtxCreate_v4 with an extra params struct.
    CUctxCreateParams ctx_params{};
    ctx_params.execAffinityParams = nullptr;
    ctx_params.numExecAffinityParams = 0;
    ctx_params.cigParams = nullptr;
    CU_CHECK(cuCtxCreate(&context, &ctx_params, 0, objc_device));
#else
    CU_CHECK(cuCtxCreate(&context, 0, objc_device));
#endif
    return opaqueFromObjcNoown<CudaContext_t, CUcontext>(context);
}

/**
 * @copydoc orteaf::internal::backend::cuda::setContext
 */
void setContext(CudaContext_t context) {
    if (context == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "setContext: context cannot be nullptr");
    }
    CUcontext objc_context = objcFromOpaqueNoown<CUcontext>(context);
    CU_CHECK(cuCtxSetCurrent(objc_context));
}

/**
 * @copydoc orteaf::internal::backend::cuda::releasePrimaryContext
 */
void releasePrimaryContext(CudaDevice_t device) {
    CUdevice objc_device = cuDeviceFromOpaque(device);
    CU_CHECK(cuDevicePrimaryCtxRelease(objc_device));
}

/**
 * @copydoc orteaf::internal::backend::cuda::releaseContext
 */
void releaseContext(CudaContext_t context) {
    if (context == nullptr) return;
    CUcontext objc_context = objcFromOpaqueNoown<CUcontext>(context);
    CU_CHECK(cuCtxDestroy(objc_context));
}

} // namespace orteaf::internal::execution::cuda::platform::wrapper

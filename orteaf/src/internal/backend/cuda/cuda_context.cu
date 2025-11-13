/**
 * @file cuda_context.cu
 * @brief Implementation of CUDA context helpers (retain/create/set/release).
 *
 * These functions wrap CUDA Driver API calls and surface errors through
 * `OrteafErrc` as `std::system_error` via `CU_CHECK`. When CUDA is disabled,
 * implementations are effectively no-ops and return nullptr where applicable.
 */
#include "orteaf/internal/backend/cuda/cuda_context.h"
#include "orteaf/internal/backend/cuda/cuda_device.h"
#include "orteaf/internal/backend/cuda/cuda_objc_bridge.h"

#include "orteaf/internal/diagnostics/error/error_impl.h"

#ifdef ORTEAF_ENABLE_CUDA
#include <cuda.h>
#include "orteaf/internal/backend/cuda/cuda_check.h"
#endif

namespace orteaf::internal::backend::cuda {

/**
 * @copydoc orteaf::internal::backend::cuda::getPrimaryContext
 */
CUcontext_t getPrimaryContext(CUdevice_t device) {
#ifdef ORTEAF_ENABLE_CUDA
    CUdevice objc_device = cuDeviceFromOpaque(device);
    CUcontext context = nullptr;
    CU_CHECK(cuDevicePrimaryCtxRetain(&context, objc_device));
    return opaqueFromObjcNoown<CUcontext_t, CUcontext>(context);
#else
    (void)device;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "CUDA backend is not available (CUDA is disabled). Called getPrimaryContext()");
#endif
}

/**
 * @copydoc orteaf::internal::backend::cuda::createContext
 */
CUcontext_t createContext(CUdevice_t device) {
#ifdef ORTEAF_ENABLE_CUDA
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
    return opaqueFromObjcNoown<CUcontext_t, CUcontext>(context);
#else
    (void)device;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "CUDA backend is not available (CUDA is disabled). Called createContext()");
#endif
}

/**
 * @copydoc orteaf::internal::backend::cuda::setContext
 */
void setContext(CUcontext_t context) {
#ifdef ORTEAF_ENABLE_CUDA
    if (context == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "setContext: context cannot be nullptr");
    }
    CUcontext objc_context = objcFromOpaqueNoown<CUcontext>(context);
    CU_CHECK(cuCtxSetCurrent(objc_context));
#else
    (void)context;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "CUDA backend is not available (CUDA is disabled). Called setContext()");
#endif
}

/**
 * @copydoc orteaf::internal::backend::cuda::releasePrimaryContext
 */
void releasePrimaryContext(CUdevice_t device) {
#ifdef ORTEAF_ENABLE_CUDA
    CUdevice objc_device = cuDeviceFromOpaque(device);
    CU_CHECK(cuDevicePrimaryCtxRelease(objc_device));
#else
    (void)device;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "CUDA backend is not available (CUDA is disabled). Called releasePrimaryContext()");
#endif
}

/**
 * @copydoc orteaf::internal::backend::cuda::releaseContext
 */
void releaseContext(CUcontext_t context) {
#ifdef ORTEAF_ENABLE_CUDA
    if (context == nullptr) return;
    CUcontext objc_context = objcFromOpaqueNoown<CUcontext>(context);
    CU_CHECK(cuCtxDestroy(objc_context));
#else
    (void)context;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "CUDA backend is not available (CUDA is disabled). Called releaseContext()");
#endif
}

} // namespace orteaf::internal::backend::cuda

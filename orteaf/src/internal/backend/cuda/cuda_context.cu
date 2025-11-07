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

#ifdef ORTEAF_ENABLE_CUDA
#include <cuda.h>
#include "orteaf/internal/backend/cuda/cuda_check.h"
#include "orteaf/internal/diagnostics/error/error_impl.h"
#endif

namespace orteaf::internal::backend::cuda {

/**
 * @copydoc orteaf::internal::backend::cuda::get_primary_context
 */
CUcontext_t get_primary_context(CUdevice_t device) {
#ifdef ORTEAF_ENABLE_CUDA
    CUdevice objc_device = cu_device_from_opaque(device);
    CUcontext context = nullptr;
    CU_CHECK(cuDevicePrimaryCtxRetain(&context, objc_device));
    return opaque_from_objc_noown<CUcontext_t, CUcontext>(context);
#else
    (void)device;
    return nullptr;
#endif
}

/**
 * @copydoc orteaf::internal::backend::cuda::create_context
 */
CUcontext_t create_context(CUdevice_t device) {
#ifdef ORTEAF_ENABLE_CUDA
    CUdevice objc_device = cu_device_from_opaque(device);
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
    return opaque_from_objc_noown<CUcontext_t, CUcontext>(context);
#else
    (void)device;
    return nullptr;
#endif
}

/**
 * @copydoc orteaf::internal::backend::cuda::set_context
 */
void set_context(CUcontext_t context) {
#ifdef ORTEAF_ENABLE_CUDA
    if (context == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::NullPointer, "set_context: context cannot be nullptr");
    }
    CUcontext objc_context = objc_from_opaque_noown<CUcontext>(context);
    CU_CHECK(cuCtxSetCurrent(objc_context));
#else
    (void)context;
#endif
}

/**
 * @copydoc orteaf::internal::backend::cuda::release_primary_context
 */
void release_primary_context(CUdevice_t device) {
#ifdef ORTEAF_ENABLE_CUDA
    CUdevice objc_device = cu_device_from_opaque(device);
    CU_CHECK(cuDevicePrimaryCtxRelease(objc_device));
#else
    (void)device;
#endif
}

/**
 * @copydoc orteaf::internal::backend::cuda::release_context
 */
void release_context(CUcontext_t context) {
#ifdef ORTEAF_ENABLE_CUDA
    if (context == nullptr) return;
    CUcontext objc_context = objc_from_opaque_noown<CUcontext>(context);
    CU_CHECK(cuCtxDestroy(objc_context));
#else
    (void)context;
#endif
}

} // namespace orteaf::internal::backend::cuda

#include "orteaf/internal/backend/cuda/cuda_context.h"
#include "orteaf/internal/backend/cuda/cuda_device.h"
#include "orteaf/internal/backend/cuda/cuda_objc_bridge.h"

#ifdef CUDA_AVAILABLE
#include <cuda.h>
#include "orteaf/internal/backend/cuda/cuda_check.h"
#endif

namespace orteaf::internal::backend::cuda {

CUcontext_t get_primary_context(CUdevice_t device) {
#ifdef CUDA_AVAILABLE
    CUdevice objc_device = cu_device_from_opaque(device);
    CUcontext context = nullptr;
    CU_CHECK(cuDevicePrimaryCtxRetain(&context, objc_device));
    return opaque_from_objc_noown<CUcontext_t, CUcontext>(context);
#else
    (void)device;
    return nullptr;
#endif
}

CUcontext_t create_context(CUdevice_t device) {
#ifdef CUDA_AVAILABLE
    CUdevice objc_device = cu_device_from_opaque(device);
    CUcontext context = nullptr;
    CU_CHECK(cuCtxCreate(&context, 0, objc_device));
    return opaque_from_objc_noown<CUcontext_t, CUcontext>(context);
#else
    (void)device;
    return nullptr;
#endif
}

void set_context(CUcontext_t context) {
#ifdef CUDA_AVAILABLE
    CUcontext objc_context = objc_from_opaque_noown<CUcontext>(context);
    CU_CHECK(cuCtxSetCurrent(objc_context));
#else
    (void)context;
#endif
}

void release_primary_context(CUdevice_t device) {
#ifdef CUDA_AVAILABLE
    CUdevice objc_device = cu_device_from_opaque(device);
    CU_CHECK(cuDevicePrimaryCtxRelease(objc_device));
#else
    (void)device;
#endif
}

void release_context(CUcontext_t context) {
#ifdef CUDA_AVAILABLE
    CUcontext objc_context = objc_from_opaque_noown<CUcontext>(context);
    CU_CHECK(cuCtxDestroy(objc_context));
#else
    (void)context;
#endif
}

} // namespace orteaf::internal::backend::cuda

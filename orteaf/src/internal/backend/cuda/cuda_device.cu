#include "orteaf/internal/backend/cuda/cuda_device.h"
#include "orteaf/internal/backend/cuda/cuda_check.h"
#include "orteaf/internal/backend/cuda/cuda_stats.h"
#include "orteaf/internal/backend/cuda/cuda_objc_bridge.h"

#ifdef CUDA_AVAILABLE
#include <cuda.h>
#endif

namespace orteaf::internal::backend::cuda {

int get_device_count() {
#ifdef CUDA_AVAILABLE
    int device_count;
    CU_CHECK(cuDeviceGetCount(&device_count));
    return device_count;
#else
    return 0;
#endif
}

CUdevice_t get_device(uint32_t device_id) {
#ifdef CUDA_AVAILABLE
    CUdevice device;
    CU_CHECK(cuDeviceGet(&device, static_cast<int>(device_id)));
    return opaque_from_cu_device(device);
#else
    (void)device_id;
    return 0;
#endif
}

ComputeCapability get_compute_capability(CUdevice_t device) {
#ifdef CUDA_AVAILABLE
    CUdevice objc_device = cu_device_from_opaque(device);
    ComputeCapability capability;
    CU_CHECK(cuDeviceGetAttribute(&capability.major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, objc_device));
    CU_CHECK(cuDeviceGetAttribute(&capability.minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, objc_device));
    return capability;
#else
    (void)device;
    return {0, 0};
#endif
}

int get_sm_count(ComputeCapability capability) {
    return capability.major * 10 + capability.minor;
}

ARCH detect_cuda_arch(ComputeCapability capability) {
    const int sm = get_sm_count(capability);
    switch (sm) {
        case 70: return ARCH::CUDA_SM70;
        case 75: return ARCH::CUDA_SM75;
        case 80: return ARCH::CUDA_SM80;
        case 86: return ARCH::CUDA_SM86;
        case 89: return ARCH::CUDA_SM89;
        case 90: return ARCH::CUDA_SM90;
        default: return ARCH::CUDA_Generic;
    }
}

} // namespace orteaf::internal::backend::cuda

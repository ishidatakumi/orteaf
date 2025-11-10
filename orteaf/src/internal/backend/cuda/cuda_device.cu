/**
 * @file cuda_device.cu
 * @brief Implementation of CUDA device helpers (count/get/capability/SM count).
 *
 * These functions call CUDA Driver API and surface errors via `CU_CHECK`,
 * which maps to `std::system_error` with `OrteafErrc`. When CUDA is disabled,
 * implementations return neutral values and perform no operations.
 */
#include "orteaf/internal/backend/cuda/cuda_device.h"
#include "orteaf/internal/backend/cuda/cuda_check.h"
#include "orteaf/internal/backend/cuda/cuda_stats.h"
#include "orteaf/internal/backend/cuda/cuda_objc_bridge.h"

#include <string>

#ifdef ORTEAF_ENABLE_CUDA
#include <cuda.h>
#endif

namespace orteaf::internal::backend::cuda {

/**
 * @copydoc orteaf::internal::backend::cuda::get_device_count
 */
int get_device_count() {
#ifdef ORTEAF_ENABLE_CUDA
    int device_count;
    CU_CHECK(cuDeviceGetCount(&device_count));
    return device_count;
#else
    return 0;
#endif
}

/**
 * @copydoc orteaf::internal::backend::cuda::get_device
 */
CUdevice_t get_device(uint32_t device_id) {
#ifdef ORTEAF_ENABLE_CUDA
    CUdevice device;
    CU_CHECK(cuDeviceGet(&device, static_cast<int>(device_id)));
    return opaque_from_cu_device(device);
#else
    (void)device_id;
    return 0;
#endif
}

/**
 * @copydoc orteaf::internal::backend::cuda::get_compute_capability
 */
ComputeCapability get_compute_capability(CUdevice_t device) {
#ifdef ORTEAF_ENABLE_CUDA
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

/**
 * @copydoc orteaf::internal::backend::cuda::get_sm_count
 */
int get_sm_count(ComputeCapability capability) {
    return capability.major * 10 + capability.minor;
}

/**
 * @copydoc orteaf::internal::backend::cuda::get_device_name
 */
std::string get_device_name(CUdevice_t device) {
#ifdef ORTEAF_ENABLE_CUDA
    CUdevice objc_device = cu_device_from_opaque(device);
    char name[256];
    CU_CHECK(cuDeviceGetName(name, sizeof(name), objc_device));
    return std::string(name);
#else
    (void)device;
    return {};
#endif
}

/**
 * @copydoc orteaf::internal::backend::cuda::get_device_vendor
 */
std::string get_device_vendor(CUdevice_t device) {
#ifdef ORTEAF_ENABLE_CUDA
    (void)device;
    return "nvidia";
#else
    (void)device;
    return {};
#endif
}

} // namespace orteaf::internal::backend::cuda

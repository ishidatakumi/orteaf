/**
 * @file cuda_device.cu
 * @brief Implementation of CUDA device helpers (count/get/capability/SM count).
 *
 * These functions call CUDA Driver API and surface errors via `CU_CHECK`,
 * which maps to `std::system_error` with `OrteafErrc`. When CUDA is disabled,
 * implementations return neutral values and perform no operations.
 */
#ifndef __CUDACC__
#error "cuda_device.cu must be compiled with a CUDA compiler (__CUDACC__ not defined)"
#endif
#include "orteaf/internal/backend/cuda/cuda_device.h"
#include "orteaf/internal/backend/cuda/cuda_check.h"
#include "orteaf/internal/backend/cuda/cuda_stats.h"
#include "orteaf/internal/backend/cuda/cuda_objc_bridge.h"

#include <string>
#include "orteaf/internal/diagnostics/error/error.h"
#include <cuda.h>

namespace orteaf::internal::backend::cuda {

/**
 * @copydoc orteaf::internal::backend::cuda::getDeviceCount
 */
int getDeviceCount() {
    int device_count;
    CU_CHECK(cuDeviceGetCount(&device_count));
    return device_count;
}

/**
 * @copydoc orteaf::internal::backend::cuda::getDevice
 */
CUdevice_t getDevice(uint32_t device_id) {
    int device_count = getDeviceCount();
    if (device_id >= static_cast<uint32_t>(device_count)) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::InvalidParameter, "getDevice: device_id out of range");
    }
    CUdevice device;
    CU_CHECK(cuDeviceGet(&device, static_cast<int>(device_id)));
    return opaqueFromCuDevice(device);
}

/**
 * @copydoc orteaf::internal::backend::cuda::getComputeCapability
 */
ComputeCapability getComputeCapability(CUdevice_t device) {
    CUdevice objc_device = cuDeviceFromOpaque(device);
    ComputeCapability capability;
    CU_CHECK(cuDeviceGetAttribute(&capability.major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, objc_device));
    CU_CHECK(cuDeviceGetAttribute(&capability.minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, objc_device));
    return capability;
}

/**
 * @copydoc orteaf::internal::backend::cuda::getSmCount
 */
int getSmCount(ComputeCapability capability) {
    return capability.major * 10 + capability.minor;
}

/**
 * @copydoc orteaf::internal::backend::cuda::getDeviceName
 */
std::string getDeviceName(CUdevice_t device) {
    CUdevice objc_device = cuDeviceFromOpaque(device);
    char name[256];
    CU_CHECK(cuDeviceGetName(name, sizeof(name), objc_device));
    return std::string(name);
}

/**
 * @copydoc orteaf::internal::backend::cuda::getDeviceVendor
 */
std::string getDeviceVendor(CUdevice_t device) {
    (void)device;
    return "nvidia";
}

} // namespace orteaf::internal::backend::cuda

/**
 * @file cuda_device.h
 * @brief CUDA device discovery, selection, and capability helpers.
 *
 * Provides thin wrappers around CUDA Driver API for enumerating devices,
 * obtaining device handles, and querying compute capability. When CUDA is
 * disabled, functions are available but return neutral values (e.g., 0 or
 * {0,0}) and perform no operations.
 */
#pragma once

#if ORTEAF_ENABLE_CUDA

#include <cstdint>
#include <string>

#include "orteaf/internal/runtime/cuda/platform/wrapper/cuda_types.h"

namespace orteaf::internal::runtime::cuda::platform::wrapper {

/**
 * @brief Bitmask flags indicating optional CUDA capabilities.
 */
enum CudaCap : uint64_t {
    /** Support for asynchronous copy (`cp.async`). */
    CapCpAsync               = 1ull << 0,
    /** Support for cluster launch features. */
    CapClusterLaunch         = 1ull << 1,
    /** Support for cooperative multi-device launches. */
    CapCoopMultiDeviceLaunch = 1ull << 2,
    /** Support for virtual memory management APIs. */
    CapVirtualMemoryMgmt     = 1ull << 3,
    /** Support for CUDA memory pools. */
    CapMemoryPools           = 1ull << 4,
};

/**
 * @brief SM compute capability (major.minor).
 */
struct ComputeCapability {
    int major;  ///< Major version (e.g., 8 for SM 8.x)
    int minor;  ///< Minor version (e.g., 0 for SM x.0)
};

/**
 * @brief Get the number of CUDA devices available.
 * @return Number of devices; 0 when CUDA is disabled.
 */
int getDeviceCount();

/**
 * @brief Get an opaque device handle for the given index.
 * @param device_id Zero-based device index
 * @return Opaque `CudaDevice_t`; 0 when CUDA is disabled.
 */
CudaDevice_t getDevice(uint32_t device_id);

/**
 * @brief Query the compute capability of a device.
 * @param device Opaque device handle
 * @return SM compute capability; {0,0} when CUDA is disabled.
 */
ComputeCapability getComputeCapability(CudaDevice_t device);

/**
 * @brief Compute a simple SM count heuristic from capability.
 * @param capability SM compute capability
 * @return Heuristic SM count used internally for sizing.
 */
int getSmCount(ComputeCapability capability);

/**
 * @brief Human-readable device name (e.g., "NVIDIA H100").
 * @param device Opaque device handle
 * @return UTF-8 device name; empty when CUDA is disabled/unavailable.
 */
std::string getDeviceName(CudaDevice_t device);

/**
 * @brief Vendor hint for architecture detection.
 * @param device Opaque device handle
 * @return Vendor string (typically "nvidia"); empty when unavailable.
 */
std::string getDeviceVendor(CudaDevice_t device);

} // namespace orteaf::internal::runtime::cuda::platform::wrapper

#endif  // ORTEAF_ENABLE_CUDA

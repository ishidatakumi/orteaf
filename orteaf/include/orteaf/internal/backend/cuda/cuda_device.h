
#pragma once

// TODO: ARCH type definition needed
// #include "orteaf/internal/backend/arch_register.h"
#include <cstdint>

using CUdevice_t = int;            // Keep ABI stable across TUs

// ABI guards (header-level, so every TU checks these)
static_assert(sizeof(CUdevice_t) == sizeof(int), "CUdevice_t must be int-sized (Driver API handle).");

namespace orteaf::internal::backend::cuda {

enum CudaCap : uint64_t {
    CapCpAsync               = 1ull << 0,
    CapClusterLaunch         = 1ull << 1,
    CapCoopMultiDeviceLaunch = 1ull << 2,
    CapVirtualMemoryMgmt     = 1ull << 3,
    CapMemoryPools           = 1ull << 4,
};

struct ComputeCapability {
    int major;
    int minor;
};

int get_device_count();
CUdevice_t get_device(uint32_t device_id);
void set_device(CUdevice_t device);
ComputeCapability get_compute_capability(CUdevice_t device);
int get_sm_count(ComputeCapability capability);
ARCH detect_cuda_arch(ComputeCapability capability);

} // namespace orteaf::internal::backend::cuda
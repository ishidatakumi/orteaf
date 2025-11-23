#include "orteaf/internal/architecture/architecture.h"
#include "orteaf/internal/architecture/cuda_detect.h"
#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/base/strong_id.h"

#include <cstdint>
#include <cstdlib>
#include <limits>

#include <gtest/gtest.h>
namespace architecture = orteaf::internal::architecture;
namespace base = orteaf::internal::base;

#if ORTEAF_ENABLE_CUDA
/// Manual test hook: set ORTEAF_EXPECT_CUDA_ARCH=Sm80 and optionally ORTEAF_EXPECT_CUDA_DEVICE_INDEX.
TEST(CudaDetect, ManualEnvironmentCheck) {
    const char* expected_env = std::getenv("ORTEAF_EXPECT_CUDA_ARCH");
    if (!expected_env) {
        GTEST_SKIP() << "Set ORTEAF_EXPECT_CUDA_ARCH to run this test on your environment.";
    }

    std::uint32_t device_index = 0;
    if (const char* index_env = std::getenv("ORTEAF_EXPECT_CUDA_DEVICE_INDEX")) {
        device_index = static_cast<std::uint32_t>(std::strtoul(index_env, nullptr, 10));
    }

    const auto arch = architecture::detectCudaArchitectureForDeviceId(base::DeviceId{device_index});
    ASSERT_NE(arch, architecture::Architecture::CudaGeneric)
        << "Generic fallback indicates CUDA backend is disabled or device index "
        << device_index << " is unavailable.";
    EXPECT_STREQ(expected_env, architecture::idOf(arch).data());
}

TEST(CudaDetect, MatchesSm80ViaComputeCapability) {
    const auto arch = architecture::detectCudaArchitecture(80, "NVIDIA");
    EXPECT_EQ(arch, architecture::Architecture::CudaSm80);
}

TEST(CudaDetect, FallsBackToGenericIfNoMatch) {
    const auto arch = architecture::detectCudaArchitecture(999, "nvidia");
    EXPECT_EQ(arch, architecture::Architecture::CudaGeneric);
}

TEST(CudaDetect, DeviceIndexOutOfRangeFallsBackToGeneric) {
    const auto arch = architecture::detectCudaArchitectureForDeviceId(
        base::DeviceId{std::numeric_limits<std::uint32_t>::max()});
    EXPECT_EQ(arch, architecture::Architecture::CudaGeneric);
}
#else
TEST(CudaDetect, DetectCudaArchitectureStillMatchesMetadataWhenCudaDisabled) {
    const auto arch = architecture::detectCudaArchitecture(80, "NVIDIA");
    EXPECT_EQ(arch, architecture::Architecture::CudaSm80);
}

TEST(CudaDetect, DetectCudaArchitectureForDeviceIdIsGenericWhenCudaDisabled) {
    const auto arch = architecture::detectCudaArchitectureForDeviceId(base::DeviceId{0});
    EXPECT_EQ(arch, architecture::Architecture::CudaGeneric);
}
#endif

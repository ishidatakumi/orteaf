#include "orteaf/internal/architecture/architecture.h"
#include "orteaf/internal/architecture/mps_detect.h"
#include "orteaf/internal/base/strong_id.h"

#include <cstdint>
#include <cstdlib>
#include <limits>
#include <iostream>
#include <gtest/gtest.h>
namespace architecture = orteaf::internal::architecture;

#if ORTEAF_ENABLE_MPS
/// Manual test hook: set ORTEAF_EXPECT_MPS_ARCH=M4 (etc) and optionally ORTEAF_EXPECT_MPS_DEVICE_INDEX.
TEST(MpsDetect, ManualEnvironmentCheck) {
    const char* expected_env = std::getenv("ORTEAF_EXPECT_MPS_ARCH");
    if (!expected_env) {
        GTEST_SKIP() << "Set ORTEAF_EXPECT_MPS_ARCH to run this test on your environment.";
    }

    std::uint32_t device_index = 0;
    if (const char* index_env = std::getenv("ORTEAF_EXPECT_MPS_DEVICE_INDEX")) {
        device_index = static_cast<std::uint32_t>(std::strtoul(index_env, nullptr, 10));
    }
    const ::orteaf::internal::base::DeviceId device_id(device_index);

    const auto arch = architecture::detectMpsArchitectureForDeviceId(device_id);
    ASSERT_NE(arch, architecture::Architecture::MpsGeneric)
        << "Generic fallback indicates Metal backend disabled or no device at index "
        << device_index;
    std::cout << "arch: " << architecture::idOf(arch).data() << std::endl;
    EXPECT_STREQ(expected_env, architecture::idOf(arch).data());
}

TEST(MpsDetect, MatchesMetalFamily) {
    const auto arch = architecture::detectMpsArchitecture("m3", "Apple");
    EXPECT_EQ(arch, architecture::Architecture::MpsM3);
}

TEST(MpsDetect, FallsBackToGenericWhenUnknown) {
    const auto arch = architecture::detectMpsArchitecture("unknown_family", "apple");
    EXPECT_EQ(arch, architecture::Architecture::MpsGeneric);
}

TEST(MpsDetect, DeviceIndexOutOfRangeFallsBackToGeneric) {
    const ::orteaf::internal::base::DeviceId device_id(std::numeric_limits<std::uint32_t>::max());
    const auto arch = architecture::detectMpsArchitectureForDeviceId(device_id);
    EXPECT_EQ(arch, architecture::Architecture::MpsGeneric);
}
#else
TEST(MpsDetect, DetectMpsArchitectureStillMatchesMetadataWhenMpsDisabled) {
    const auto arch = architecture::detectMpsArchitecture("m3", "Apple");
    EXPECT_EQ(arch, architecture::Architecture::MpsM3);
}

TEST(MpsDetect, DetectMpsArchitectureForDeviceIdIsGenericWhenMpsDisabled) {
    const auto arch = architecture::detectMpsArchitectureForDeviceId(::orteaf::internal::base::DeviceId{0});
    EXPECT_EQ(arch, architecture::Architecture::MpsGeneric);
}
#endif

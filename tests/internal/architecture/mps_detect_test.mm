#include "orteaf/internal/architecture/architecture.h"
#include "orteaf/internal/architecture/mps_detect.h"

#include <cstdint>
#include <cstdlib>
#include <limits>
#include <iostream>
#include <gtest/gtest.h>
namespace architecture = orteaf::internal::architecture;

/// Manual test hook: set ORTEAF_EXPECT_MPS_ARCH=m4 (etc) and optionally ORTEAF_EXPECT_MPS_DEVICE_INDEX.
TEST(MpsDetect, ManualEnvironmentCheck) {
    const char* expected_env = std::getenv("ORTEAF_EXPECT_MPS_ARCH");
    if (!expected_env) {
        GTEST_SKIP() << "Set ORTEAF_EXPECT_MPS_ARCH to run this test on your environment.";
    }

    std::uint32_t device_index = 0;
    if (const char* index_env = std::getenv("ORTEAF_EXPECT_MPS_DEVICE_INDEX")) {
        device_index = static_cast<std::uint32_t>(std::strtoul(index_env, nullptr, 10));
    }

    const auto arch = architecture::detect_mps_architecture_for_device_index(device_index);
    ASSERT_NE(arch, architecture::Architecture::mps_generic)
        << "Generic fallback indicates Metal backend disabled or no device at index "
        << device_index;
    std::cout << "arch: " << architecture::IdOf(arch).data() << std::endl;
    EXPECT_STREQ(expected_env, architecture::IdOf(arch).data());
}

TEST(MpsDetect, MatchesMetalFamily) {
    const auto arch = architecture::detect_mps_architecture("m3", "Apple");
    EXPECT_EQ(arch, architecture::Architecture::mps_m3);
}

TEST(MpsDetect, FallsBackToGenericWhenUnknown) {
    const auto arch = architecture::detect_mps_architecture("unknown_family", "apple");
    EXPECT_EQ(arch, architecture::Architecture::mps_generic);
}

TEST(MpsDetect, DeviceIndexOutOfRangeFallsBackToGeneric) {
    const auto arch = architecture::detect_mps_architecture_for_device_index(std::numeric_limits<std::uint32_t>::max());
    EXPECT_EQ(arch, architecture::Architecture::mps_generic);
}

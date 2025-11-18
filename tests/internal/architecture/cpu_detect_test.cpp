#include "orteaf/internal/architecture/architecture.h"
#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/architecture/cpu_detect.h"

#include <cstdlib>
#include <iostream>
#include <gtest/gtest.h>
namespace architecture = orteaf::internal::architecture;
namespace backend = orteaf::internal::backend;

/// Manual test hook: set ORTEAF_EXPECT_CPU_ARCH=zen4 (or other ID) to assert your environment.
TEST(CpuDetect, ManualEnvironmentCheck) {
    const char* expected_env = std::getenv("ORTEAF_EXPECT_CPU_ARCH");
    if (!expected_env) {
        GTEST_SKIP() << "Set ORTEAF_EXPECT_CPU_ARCH to run this test on your environment.";
    }
    std::cout << "expected_env: " << expected_env << std::endl;
    const auto arch = architecture::detectCpuArchitecture();
    std::cout << "arch: " << architecture::idOf(arch).data() << std::endl;
    EXPECT_STREQ(expected_env, architecture::idOf(arch).data());
}

TEST(CpuDetect, ReportsCpuBackendArchitecture) {
    const auto arch = architecture::detectCpuArchitecture();
    EXPECT_EQ(architecture::backendOf(arch), backend::Backend::cpu);
    EXPECT_GE(static_cast<std::uint16_t>(arch), 0);
}

TEST(CpuDetect, GenericIsAlwaysValid) {
    constexpr auto generic = architecture::Architecture::cpu_generic;
    EXPECT_TRUE(architecture::isValidIndex(static_cast<std::size_t>(generic)));
}

#include <gtest/gtest.h>
#include "orteaf/internal/runtime/manager/cpu/device_manager.h"
#include "orteaf/internal/architecture/architecture.h"
#include "orteaf/internal/architecture/cpu_detect.h"

#include <cstdlib>

namespace runtime = orteaf::internal::runtime;
namespace cpu_rt = orteaf::internal::runtime::cpu;
namespace architecture = orteaf::internal::architecture;

class CpuDeviceManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        cpu_rt::CpuDeviceManager.initializeDevices();
    }
    void TearDown() override {
        cpu_rt::CpuDeviceManager.shutdown();
    }
};

TEST_F(CpuDeviceManagerTest, GetDeviceCount) {
    EXPECT_EQ(cpu_rt::CpuDeviceManager.getDeviceCount(), 1);
}

#define ORTEAF_CPU_ENV_VAR "ORTEAF_EXPECT_CPU_MANAGER_ARCH"

/// Manual test hook: set ORTEAF_EXPECT_CPU_MANAGER_ARCH=apple_m4_pro (など) で検証。
TEST_F(CpuDeviceManagerTest, ManualEnvironmentCheck) {
    const char* expected_env = std::getenv(ORTEAF_CPU_ENV_VAR);
    if (!expected_env) {
        GTEST_SKIP() << "Set " ORTEAF_CPU_ENV_VAR " to run this test on your environment.";
    }
    const auto arch = cpu_rt::CpuDeviceManager.getArch(runtime::DeviceId{0});
    EXPECT_STREQ(expected_env, architecture::idOf(arch).data());
}

TEST_F(CpuDeviceManagerTest, GetArchitecture) {
    runtime::DeviceId device_id{0};
    EXPECT_EQ(cpu_rt::CpuDeviceManager.getArch(device_id), architecture::detectCpuArchitecture());
}

TEST_F(CpuDeviceManagerTest, GetIsAlive) {
    runtime::DeviceId device_id{0};
    EXPECT_TRUE(cpu_rt::CpuDeviceManager.isAlive(device_id));
}

#include "orteaf/internal/architecture/architecture.h"
#include "orteaf/internal/architecture/cpu_detect.h"
#include "orteaf/internal/execution/cpu/manager/cpu_device_manager.h"
#include <gtest/gtest.h>
#include <system_error>

#include <cstdlib>

namespace base = orteaf::internal::base;
namespace cpu_rt = orteaf::internal::execution::cpu::manager;
namespace architecture = orteaf::internal::architecture;

class CpuDeviceManagerTest : public ::testing::Test {
protected:
  void SetUp() override { cpu_rt::GetCpuDeviceManager().shutdown(); }
  void TearDown() override { cpu_rt::GetCpuDeviceManager().shutdown(); }
};

TEST_F(CpuDeviceManagerTest, InitializeDevicesPopulatesState) {
  auto &manager = cpu_rt::GetCpuDeviceManager();
  EXPECT_EQ(manager.getDeviceCount(), 0u);

  manager.initializeDevices();
  EXPECT_EQ(manager.getDeviceCount(), 1u);
  EXPECT_TRUE(manager.isAlive(base::DeviceHandle{0}));
  EXPECT_EQ(manager.getArch(base::DeviceHandle{0}),
            architecture::detectCpuArchitecture());
}

TEST_F(CpuDeviceManagerTest, ShutdownClearsState) {
  auto &manager = cpu_rt::GetCpuDeviceManager();
  manager.initializeDevices();
  manager.shutdown();

  EXPECT_EQ(manager.getDeviceCount(), 0u);
  EXPECT_THROW(manager.getArch(base::DeviceHandle{0}), std::system_error);
  EXPECT_THROW(manager.isAlive(base::DeviceHandle{0}), std::system_error);
}

#define ORTEAF_CPU_ENV_VAR "ORTEAF_EXPECT_CPU_MANAGER_ARCH"

/// Manual test hook: set ORTEAF_EXPECT_CPU_MANAGER_ARCH=AppleM4Pro (など)
/// で検証。
TEST_F(CpuDeviceManagerTest, ManualEnvironmentCheck) {
  const char *expected_env = std::getenv(ORTEAF_CPU_ENV_VAR);
  if (!expected_env) {
    GTEST_SKIP() << "Set " ORTEAF_CPU_ENV_VAR
                    " to run this test on your environment.";
  }
  const auto arch =
      cpu_rt::GetCpuDeviceManager().getArch(base::DeviceHandle{0});
  EXPECT_STREQ(expected_env, architecture::idOf(arch).data());
}

TEST_F(CpuDeviceManagerTest, GetArchitectureMatchesDetector) {
  auto &manager = cpu_rt::GetCpuDeviceManager();
  manager.initializeDevices();
  EXPECT_EQ(manager.getArch(base::DeviceHandle{0}),
            architecture::detectCpuArchitecture());
}

TEST_F(CpuDeviceManagerTest, IsAliveReflectsInitialization) {
  auto &manager = cpu_rt::GetCpuDeviceManager();
  manager.initializeDevices();
  EXPECT_TRUE(manager.isAlive(base::DeviceHandle{0}));
  manager.shutdown();
  EXPECT_THROW(manager.isAlive(base::DeviceHandle{0}), std::system_error);
}
#include <system_error>

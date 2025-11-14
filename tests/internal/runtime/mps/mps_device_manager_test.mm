#include <gtest/gtest.h>

#include <cstdlib>
#include <system_error>

#include "orteaf/internal/runtime/manager/mps/mps_device_manager.h"
#include "orteaf/internal/architecture/architecture.h"

namespace mps_rt = orteaf::internal::runtime::mps;
namespace architecture = orteaf::internal::architecture;
namespace backend = orteaf::internal::backend;
namespace base = orteaf::internal::base;

class MpsDeviceManagerTest : public ::testing::Test {
protected:
    void SetUp() override { mps_rt::GetMpsDeviceManager().shutdown(); }
    void TearDown() override { mps_rt::GetMpsDeviceManager().shutdown(); }
};

#define ORTEAF_MPS_ENV_COUNT "ORTEAF_EXPECT_MPS_DEVICE_COUNT"
#define ORTEAF_MPS_ENV_ARCH "ORTEAF_EXPECT_MPS_DEVICE_ARCH"

namespace {

bool shouldRunHardwareTests() {
    const char* expected_env = std::getenv(ORTEAF_MPS_ENV_COUNT);
    return expected_env != nullptr && std::stoi(expected_env) > 0;
}

}  // namespace

TEST_F(MpsDeviceManagerTest, GetDeviceCount) {
    const char* expected_env = std::getenv(ORTEAF_MPS_ENV_COUNT);
    if (!expected_env) {
        GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_COUNT " to run this test on your environment.";
    }
    auto& manager = mps_rt::GetMpsDeviceManager();
    manager.initializeDevices();
    const auto count = manager.getDeviceCount();
    EXPECT_EQ(std::stoi(expected_env), count);
}

TEST_F(MpsDeviceManagerTest, GetDevice) {
    if (!shouldRunHardwareTests()) {
        GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_COUNT " to a positive integer to run this test.";
    }
    auto& manager = mps_rt::GetMpsDeviceManager();
    manager.initializeDevices();
    base::DeviceId device_id{0};
    backend::mps::MPSDevice_t device = manager.getDevice(device_id);
    EXPECT_NE(device, nullptr);
}

TEST_F(MpsDeviceManagerTest, GetArch) {
    const char* expected_env = std::getenv(ORTEAF_MPS_ENV_ARCH);
    if (!expected_env) {
        GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_ARCH " to run this test on your environment.";
    }
    auto& manager = mps_rt::GetMpsDeviceManager();
    manager.initializeDevices();
    const auto arch = manager.getArch(base::DeviceId{0});
    EXPECT_STREQ(expected_env, architecture::idOf(arch).data());
}

TEST_F(MpsDeviceManagerTest, GetIsAlive) {
    if (!shouldRunHardwareTests()) {
        GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_COUNT " to a positive integer to run this test.";
    }
    auto& manager = mps_rt::GetMpsDeviceManager();
    manager.initializeDevices();
    base::DeviceId device_id{0};
    EXPECT_TRUE(manager.isAlive(device_id));
}

TEST_F(MpsDeviceManagerTest, OutOfRangeDeviceIdFails) {
    if (!shouldRunHardwareTests()) {
        GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_COUNT " to a positive integer to run this test.";
    }
    auto& manager = mps_rt::GetMpsDeviceManager();
    manager.initializeDevices();
    const auto invalid = base::DeviceId{static_cast<std::uint32_t>(manager.getDeviceCount())};
    EXPECT_THROW(manager.getDevice(invalid), std::system_error);
    EXPECT_THROW(manager.getArch(invalid), std::system_error);
    EXPECT_FALSE(manager.isAlive(invalid));
}

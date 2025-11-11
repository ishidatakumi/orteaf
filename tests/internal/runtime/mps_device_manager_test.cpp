#include <gtest/gtest.h>
#include "orteaf/internal/runtime/manager/mps/mps_device_manager.h"
#include "orteaf/internal/architecture/architecture.h"
#include "orteaf/internal/architecture/mps_detect.h"

#include <cstdlib>

namespace mps_rt = orteaf::internal::runtime::mps;
namespace architecture = orteaf::internal::architecture;
namespace backend = orteaf::internal::backend;
namespace base = orteaf::internal::base;

class MpsDeviceManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mps_rt::MpsDeviceManager.initializeDevices();
    }
    void TearDown() override {
        mps_rt::MpsDeviceManager.shutdown();
    }
};

#define ORTEAF_MPS_ENV_COUNT "ORTEAF_EXPECT_MPS_DEVICE_COUNT"

TEST_F(MpsDeviceManagerTest, GetDeviceCount) {
    const char* expected_env = std::getenv(ORTEAF_MPS_ENV_COUNT);
    if (!expected_env) {
        GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_COUNT " to run this test on your environment.";
    }
    const auto count = mps_rt::MpsDeviceManager.getDeviceCount();
    EXPECT_EQ(std::stoi(expected_env), count);
}

TEST_F(MpsDeviceManagerTest, GetDevice) {
    backend::mps::MPSDevice_t device = backend::mps::getDevice();
    EXPECT_NE(device, nullptr);
}

#define ORTEAF_MPS_ENV_ARCH "ORTEAF_EXPECT_MPS_DEVICE_ARCH"

TEST_F(MpsDeviceManagerTest, GetArch) {
    const char* expected_env = std::getenv(ORTEAF_MPS_ENV_ARCH);
    if (!expected_env) {
        GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_ARCH " to run this test on your environment.";
    }
    const auto arch = mps_rt::MpsDeviceManager.getArch(base::DeviceId{0});
    EXPECT_STREQ(expected_env, architecture::idOf(arch).data());
}

TEST_F(MpsDeviceManagerTest, GetIsAlive) {
    base::DeviceId device_id{0};
    EXPECT_TRUE(mps_rt::MpsDeviceManager.isAlive(device_id));
}

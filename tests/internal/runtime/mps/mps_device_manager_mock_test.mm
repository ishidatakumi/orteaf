#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <system_error>

#include "orteaf/internal/runtime/manager/mps/mps_device_manager.h"
#include "orteaf/internal/architecture/architecture.h"
#include "tests/internal/runtime/mps/testing/backend_mock.h"

namespace architecture = orteaf::internal::architecture;
namespace backend = orteaf::internal::backend;
namespace base = orteaf::internal::base;
namespace mps_rt = orteaf::internal::runtime::mps;
namespace test_mps = orteaf::tests::runtime::mps;

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace {

using MockMpsDeviceManager = mps_rt::MpsDeviceManager<test_mps::MpsBackendOpsMockAdapter>;

backend::mps::MPSDevice_t makeFakeDevice(std::uintptr_t value) {
    return reinterpret_cast<backend::mps::MPSDevice_t>(value);
}

}  // namespace

class MpsDeviceManagerMockTest : public ::testing::Test {
protected:
    MpsDeviceManagerMockTest()
        : guard_(mock_) {}

    void TearDown() override {
        manager_.shutdown();
        test_mps::MpsBackendOpsMockRegistry::unbind(mock_);
    }

    NiceMock<test_mps::MpsBackendOpsMock> mock_;
    test_mps::MpsBackendOpsMockRegistry::Guard guard_;
    MockMpsDeviceManager manager_;
};

TEST_F(MpsDeviceManagerMockTest, InitializesDevicesFromBackendOps) {
    const auto device0 = makeFakeDevice(0x1);
    const auto device1 = makeFakeDevice(0x2);

    EXPECT_CALL(mock_, getDeviceCount()).WillOnce(Return(2));
    EXPECT_CALL(mock_, getDevice(0)).WillOnce(Return(device0));
    EXPECT_CALL(mock_, detectArchitecture(base::DeviceId{0}))
        .WillOnce(Return(architecture::Architecture::mps_m3));
    EXPECT_CALL(mock_, getDevice(1)).WillOnce(Return(device1));
    EXPECT_CALL(mock_, detectArchitecture(base::DeviceId{1}))
        .WillOnce(Return(architecture::Architecture::mps_m4));

    manager_.initializeDevices();
    EXPECT_EQ(manager_.getDeviceCount(), 2u);
    EXPECT_EQ(manager_.getDevice(base::DeviceId{0}), device0);
    EXPECT_EQ(manager_.getDevice(base::DeviceId{1}), device1);
    EXPECT_EQ(manager_.getArch(base::DeviceId{0}), architecture::Architecture::mps_m3);
    EXPECT_EQ(manager_.getArch(base::DeviceId{1}), architecture::Architecture::mps_m4);
    EXPECT_TRUE(manager_.isAlive(base::DeviceId{0}));
    EXPECT_TRUE(manager_.isAlive(base::DeviceId{1}));

    EXPECT_CALL(mock_, releaseDevice(device0)).Times(1);
    EXPECT_CALL(mock_, releaseDevice(device1)).Times(1);
    manager_.shutdown();
    EXPECT_EQ(manager_.getDeviceCount(), 0u);
}

TEST_F(MpsDeviceManagerMockTest, ZeroDevicesStillMarksInitialized) {
    EXPECT_CALL(mock_, getDeviceCount()).WillOnce(Return(0));
    manager_.initializeDevices();
    EXPECT_EQ(manager_.getDeviceCount(), 0u);
    EXPECT_THROW(manager_.getDevice(base::DeviceId{0}), std::system_error);
}

TEST_F(MpsDeviceManagerMockTest, InvalidDeviceIdThrows) {
    const auto device = makeFakeDevice(0x3);
    EXPECT_CALL(mock_, getDeviceCount()).WillOnce(Return(1));
    EXPECT_CALL(mock_, getDevice(0)).WillOnce(Return(device));
    EXPECT_CALL(mock_, detectArchitecture(base::DeviceId{0}))
        .WillOnce(Return(architecture::Architecture::mps_m3));
    manager_.initializeDevices();

    EXPECT_THROW(manager_.getDevice(base::DeviceId{1}), std::system_error);
    EXPECT_FALSE(manager_.isAlive(base::DeviceId{1}));

    EXPECT_CALL(mock_, releaseDevice(device)).Times(1);
    manager_.shutdown();
}

TEST_F(MpsDeviceManagerMockTest, NullDevicesRemainInactive) {
    const auto device0 = makeFakeDevice(0x5);

    EXPECT_CALL(mock_, getDeviceCount()).WillOnce(Return(2));
    EXPECT_CALL(mock_, getDevice(0)).WillOnce(Return(device0));
    EXPECT_CALL(mock_, detectArchitecture(base::DeviceId{0}))
        .WillOnce(Return(architecture::Architecture::mps_m3));
    EXPECT_CALL(mock_, getDevice(1)).WillOnce(Return(nullptr));
    EXPECT_CALL(mock_, detectArchitecture(base::DeviceId{1})).Times(0);

    manager_.initializeDevices();
    EXPECT_TRUE(manager_.isAlive(base::DeviceId{0}));
    EXPECT_FALSE(manager_.isAlive(base::DeviceId{1}));
    EXPECT_THROW(manager_.getDevice(base::DeviceId{1}), std::system_error);
    EXPECT_THROW(manager_.getArch(base::DeviceId{1}), std::system_error);

    EXPECT_CALL(mock_, releaseDevice(device0)).Times(1);
    manager_.shutdown();
}

TEST_F(MpsDeviceManagerMockTest, AccessBeforeInitializationThrows) {
    EXPECT_THROW(manager_.getDevice(base::DeviceId{0}), std::system_error);
    EXPECT_THROW(manager_.getArch(base::DeviceId{0}), std::system_error);
    EXPECT_FALSE(manager_.isAlive(base::DeviceId{0}));
}

TEST_F(MpsDeviceManagerMockTest, ReinitializeReleasesPreviousDevices) {
    const auto device0 = makeFakeDevice(0x10);
    const auto device1 = makeFakeDevice(0x20);

    {
        ::testing::InSequence seq;
        EXPECT_CALL(mock_, getDeviceCount()).WillOnce(Return(1));
        EXPECT_CALL(mock_, getDevice(0)).WillOnce(Return(device0));
        EXPECT_CALL(mock_, detectArchitecture(base::DeviceId{0}))
            .WillOnce(Return(architecture::Architecture::mps_m3));
        EXPECT_CALL(mock_, releaseDevice(device0)).Times(1);
        EXPECT_CALL(mock_, getDeviceCount()).WillOnce(Return(1));
        EXPECT_CALL(mock_, getDevice(0)).WillOnce(Return(device1));
        EXPECT_CALL(mock_, detectArchitecture(base::DeviceId{0}))
            .WillOnce(Return(architecture::Architecture::mps_m4));
    }

    manager_.initializeDevices();
    manager_.initializeDevices();

    EXPECT_EQ(manager_.getDevice(base::DeviceId{0}), device1);
    EXPECT_EQ(manager_.getArch(base::DeviceId{0}), architecture::Architecture::mps_m4);

    EXPECT_CALL(mock_, releaseDevice(device1)).Times(1);
    manager_.shutdown();
}

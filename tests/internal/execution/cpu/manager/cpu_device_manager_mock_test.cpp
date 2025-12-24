#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <system_error>

#include "orteaf/internal/architecture/architecture.h"
#include "orteaf/internal/execution/cpu/manager/cpu_device_manager.h"
#include "tests/internal/testing/static_mock.h"

namespace architecture = orteaf::internal::architecture;
namespace base = orteaf::internal::base;
namespace cpu_rt = orteaf::internal::execution::cpu::manager;
namespace test = orteaf::tests;
using ::testing::NiceMock;

namespace {

struct CpuBackendOpsMock {
  MOCK_METHOD(architecture::Architecture, detectArchitecture, ());
};

using CpuBackendOpsMockRegistry = test::StaticMockRegistry<CpuBackendOpsMock>;

struct CpuBackendOpsMockAdapter {
  static architecture::Architecture detectArchitecture() {
    return CpuBackendOpsMockRegistry::get().detectArchitecture();
  }
};

using MockCpuDeviceManager = cpu_rt::CpuDeviceManager<CpuBackendOpsMockAdapter>;

} // namespace

class CpuDeviceManagerMockTest : public ::testing::Test {
protected:
  CpuDeviceManagerMockTest() : guard_(mock_) {}

  void TearDown() override {
    manager_.shutdown();
    CpuBackendOpsMockRegistry::unbind(mock_);
  }

  NiceMock<CpuBackendOpsMock> mock_;
  CpuBackendOpsMockRegistry::Guard guard_;
  MockCpuDeviceManager manager_;
};

TEST_F(CpuDeviceManagerMockTest, InitializesWithBackendArch) {
  EXPECT_EQ(manager_.getDeviceCount(), 0u);

  EXPECT_CALL(mock_, detectArchitecture())
      .WillOnce(::testing::Return(architecture::Architecture::CpuZen4));
  manager_.initializeDevices();

  EXPECT_EQ(manager_.getDeviceCount(), 1u);
  EXPECT_EQ(manager_.getArch(base::DeviceHandle{0}),
            architecture::Architecture::CpuZen4);
  EXPECT_TRUE(manager_.isAlive(base::DeviceHandle{0}));
}

TEST_F(CpuDeviceManagerMockTest, DoubleInitializeDoesNotRedetect) {
  EXPECT_CALL(mock_, detectArchitecture())
      .WillOnce(::testing::Return(architecture::Architecture::CpuZen4));
  manager_.initializeDevices();

  manager_.initializeDevices(); // no additional expectation
  SUCCEED();
}

TEST_F(CpuDeviceManagerMockTest, ShutdownClearsState) {
  EXPECT_CALL(mock_, detectArchitecture())
      .WillOnce(::testing::Return(architecture::Architecture::CpuZen4));
  manager_.initializeDevices();
  manager_.shutdown();

  EXPECT_EQ(manager_.getDeviceCount(), 0u);
  EXPECT_THROW(manager_.isAlive(base::DeviceHandle{0}), std::system_error);
  EXPECT_THROW(manager_.getArch(base::DeviceHandle{0}), std::system_error);
}

TEST_F(CpuDeviceManagerMockTest, InvalidDeviceIdThrows) {
  EXPECT_CALL(mock_, detectArchitecture())
      .WillOnce(::testing::Return(architecture::Architecture::CpuZen4));
  manager_.initializeDevices();

  EXPECT_THROW(manager_.getArch(base::DeviceHandle{1}), std::system_error);
  EXPECT_THROW(manager_.isAlive(base::DeviceHandle{1}), std::system_error);
}

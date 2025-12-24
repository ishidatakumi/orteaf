#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <system_error>
#include <vector>

#include <orteaf/internal/architecture/architecture.h>
#include <orteaf/internal/diagnostics/error/error.h>
#include <orteaf/internal/execution/mps/manager/mps_device_manager.h>
#include <tests/internal/execution/mps/manager/testing/backend_ops_provider.h>
#include <tests/internal/execution/mps/manager/testing/manager_test_fixture.h>
#include <tests/internal/testing/error_assert.h>

namespace architecture = orteaf::internal::architecture;
namespace base = orteaf::internal::base;
namespace diag_error = orteaf::internal::diagnostics::error;
namespace mps_rt = orteaf::internal::execution::mps::manager;
namespace mps_wrapper = orteaf::internal::execution::mps::platform::wrapper;
namespace testing_mps = orteaf::tests::execution::mps::testing;

using orteaf::tests::ExpectError;

#define ORTEAF_MPS_ENV_COUNT "ORTEAF_EXPECT_MPS_DEVICE_COUNT"
#define ORTEAF_MPS_ENV_ARCH "ORTEAF_EXPECT_MPS_DEVICE_ARCH"

namespace {

mps_wrapper::MpsDevice_t makeDevice(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MpsDevice_t>(value);
}

mps_wrapper::MpsCommandQueue_t makeQueue(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MpsCommandQueue_t>(value);
}

mps_rt::MpsDeviceManager::Config makeConfig(
    mps_rt::MpsDeviceManager::SlowOps *ops) {
  mps_rt::MpsDeviceManager::Config config{};
  config.ops = ops;
  return config;
}

template <class Provider>
class MpsDeviceManagerTypedTest
    : public testing_mps::RuntimeManagerFixture<Provider,
                                                mps_rt::MpsDeviceManager> {
protected:
  using Base =
      testing_mps::RuntimeManagerFixture<Provider, mps_rt::MpsDeviceManager>;

  mps_rt::MpsDeviceManager &manager() { return Base::manager(); }
  auto &adapter() { return Base::adapter(); }

  void onPreManagerTearDown() override { manager().shutdown(); }
};

#if ORTEAF_ENABLE_MPS
using ProviderTypes = ::testing::Types<testing_mps::MockBackendOpsProvider,
                                       testing_mps::RealBackendOpsProvider>;
#else
using ProviderTypes = ::testing::Types<testing_mps::MockBackendOpsProvider>;
#endif

} // namespace

TYPED_TEST_SUITE(MpsDeviceManagerTypedTest, ProviderTypes);

// =============================================================================
// Access Before Initialization Tests
// =============================================================================

TYPED_TEST(MpsDeviceManagerTypedTest, AccessBeforeInitializationThrows) {
  auto &manager = this->manager();

  // Act & Assert: All accessors throw InvalidState before initialization
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.acquire(base::DeviceHandle{0}); });
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.getArch(base::DeviceHandle{0}); });
  EXPECT_FALSE(manager.isAliveForTest(base::DeviceHandle{0}));
  EXPECT_FALSE(manager.isAliveForTest(base::DeviceHandle{0}));
}

// =============================================================================
// Initialization Tests
// =============================================================================

TYPED_TEST(MpsDeviceManagerTypedTest, InitializeMarksManagerInitialized) {
  auto &manager = this->manager();
  int expected_count = -1;

  // Arrange: Setup mock expectations
  if constexpr (!TypeParam::is_mock) {
    const char *expected_env = std::getenv(ORTEAF_MPS_ENV_COUNT);
    if (!expected_env || std::stoi(expected_env) <= 0) {
      GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_COUNT
                      " to a positive integer to run this test.";
    }
    expected_count = std::stoi(expected_env);
  } else {
    const auto device0 = makeDevice(0xCAFE);
    this->adapter().expectGetDeviceCount(1);
    this->adapter().expectGetDevices({{0, device0}});
    this->adapter().expectDetectArchitectures({
        {base::DeviceHandle{0}, architecture::Architecture::MpsM3},
    });
    this->adapter().expectReleaseDevices({device0});
    expected_count = 1;
  }

  // Act
  manager.configure(makeConfig(this->getOps()));

  // Assert
  EXPECT_TRUE(manager.isInitializedForTest());
  EXPECT_EQ(manager.payloadPoolSizeForTest(), manager.getDeviceCountForTest());
  if (expected_count >= 0) {
    EXPECT_EQ(manager.getDeviceCountForTest(),
              static_cast<std::size_t>(expected_count));
  }

  // Cleanup
  manager.shutdown();
  EXPECT_FALSE(manager.isInitializedForTest());
  EXPECT_EQ(manager.payloadPoolSizeForTest(), 0u);
}

TYPED_TEST(MpsDeviceManagerTypedTest, InitializeWithNullOpsThrows) {
  auto &manager = this->manager();

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { manager.configure(makeConfig(nullptr)); });
}

TYPED_TEST(MpsDeviceManagerTypedTest, InitializeWithZeroDevicesSucceeds) {
  if constexpr (!TypeParam::is_mock) {
    GTEST_SKIP() << "Mock-only test";
  }
  auto &manager = this->manager();

  // Arrange
  this->adapter().expectGetDeviceCount(0);

  // Act
  manager.configure(makeConfig(this->getOps()));

  // Assert
  EXPECT_TRUE(manager.isInitializedForTest());
  EXPECT_EQ(manager.payloadPoolSizeForTest(), 0u);
  EXPECT_EQ(manager.getDeviceCountForTest(), 0u);
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { (void)manager.acquire(base::DeviceHandle{0}); });

  // Cleanup
  manager.shutdown();
}

// =============================================================================
// Device Access Tests
// =============================================================================

TYPED_TEST(MpsDeviceManagerTypedTest, GetDeviceReturnsRegisteredHandle) {
  auto &manager = this->manager();

  // Arrange
  std::vector<mps_wrapper::MpsDevice_t> expected_handles;
  int expected_count = -1;
  if (const char *expected_env = std::getenv(ORTEAF_MPS_ENV_COUNT)) {
    expected_count = std::stoi(expected_env);
  }

  expected_handles = {makeDevice(0xBEEF), makeDevice(0xC001)};
  const int mock_count = static_cast<int>(expected_handles.size());
  this->adapter().expectGetDeviceCount(mock_count);
  this->adapter().expectGetDevices({
      {0, expected_handles[0]},
      {1, expected_handles[1]},
  });
  this->adapter().expectDetectArchitectures({
      {base::DeviceHandle{0}, architecture::Architecture::MpsM3},
      {base::DeviceHandle{1}, architecture::Architecture::MpsM4},
  });
  this->adapter().expectReleaseDevices(
      {expected_handles[0], expected_handles[1]});

  // Act
  manager.configure(makeConfig(this->getOps()));
  const auto count = manager.getDeviceCountForTest();
  if (expected_count >= 0) {
    EXPECT_EQ(count, static_cast<std::size_t>(expected_count));
  }
  if (count == 0u) {
    GTEST_SKIP() << "No MPS devices available";
  }

  // Assert: Verify each device
  for (std::uint32_t idx = 0; idx < count; ++idx) {
    const auto device = manager.acquire(base::DeviceHandle{idx});
    auto *resource = device.payloadPtr();
    EXPECT_NE(resource, nullptr);
    EXPECT_EQ(resource->device != nullptr, static_cast<bool>(device));
    if constexpr (TypeParam::is_mock) {
      EXPECT_EQ(resource->device, expected_handles[idx]);
      const auto expected_arch = (idx == 0) ? architecture::Architecture::MpsM3
                                            : architecture::Architecture::MpsM4;
      EXPECT_EQ(resource->arch, expected_arch);
    } else {
      EXPECT_TRUE(device);
      if (expected_count >= 0 && idx == 0) {
        EXPECT_NE(resource->arch, architecture::Architecture::MpsGeneric);
      }
    }
  }

  // Cleanup
  manager.shutdown();
}

TYPED_TEST(MpsDeviceManagerTypedTest, GetArchMatchesReportedArchitecture) {
  auto &manager = this->manager();

  // Arrange
  const char *expected_arch_env = nullptr;
  if constexpr (TypeParam::is_mock) {
    const auto device0 = makeDevice(0xAB);
    const auto device1 = makeDevice(0xCD);
    this->adapter().expectGetDeviceCount(2);
    this->adapter().expectGetDevices({{0, device0}, {1, device1}});
    this->adapter().expectDetectArchitectures({
        {base::DeviceHandle{0}, architecture::Architecture::MpsM4},
        {base::DeviceHandle{1}, architecture::Architecture::MpsM3},
    });
    this->adapter().expectReleaseDevices({device0, device1});
  } else {
    expected_arch_env = std::getenv(ORTEAF_MPS_ENV_ARCH);
  }

  // Act
  manager.configure(makeConfig(this->getOps()));
  const auto count = manager.getDeviceCountForTest();
  if (count == 0u) {
    GTEST_SKIP() << "No MPS devices available";
  }

  // Assert: Verify architecture for each device
  for (std::uint32_t idx = 0; idx < count; ++idx) {
    const auto arch = manager.getArch(base::DeviceHandle{idx});
    const auto device = manager.acquire(base::DeviceHandle{idx});
    auto *resource = device.payloadPtr();
    EXPECT_NE(resource, nullptr);
    if constexpr (TypeParam::is_mock) {
      const auto expected_arch = (idx == 0) ? architecture::Architecture::MpsM4
                                            : architecture::Architecture::MpsM3;
      EXPECT_EQ(arch, expected_arch);
      EXPECT_EQ(resource->arch, expected_arch);
      EXPECT_TRUE(resource->device != nullptr);
    } else if (expected_arch_env && *expected_arch_env != '\0' && idx == 0) {
      EXPECT_STREQ(expected_arch_env, architecture::idOf(arch).data());
      EXPECT_STREQ(expected_arch_env,
                   architecture::idOf(resource->arch).data());
    } else {
      EXPECT_FALSE(architecture::idOf(arch).empty());
      EXPECT_FALSE(architecture::idOf(resource->arch).empty());
    }
  }

  // Cleanup
  manager.shutdown();
}

// =============================================================================
// Invalid Device ID Tests
// =============================================================================

TYPED_TEST(MpsDeviceManagerTypedTest, InvalidDeviceIdRejectsAccess) {
  auto &manager = this->manager();

  // Arrange
  const auto device0 = makeDevice(0x77);
  this->adapter().expectGetDeviceCount(1);
  this->adapter().expectGetDevices({{0, device0}});
  this->adapter().expectDetectArchitectures({
      {base::DeviceHandle{0}, architecture::Architecture::MpsM3},
  });
  this->adapter().expectReleaseDevices({device0});

  manager.configure(makeConfig(this->getOps()));
  const auto invalid = base::DeviceHandle{
      static_cast<std::uint32_t>(manager.getDeviceCountForTest() + 1)};

  // Act & Assert: Invalid ID throws
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { (void)manager.acquire(invalid); });
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { (void)manager.getArch(invalid); });
  EXPECT_FALSE(manager.isAliveForTest(invalid));

  // Cleanup
  manager.shutdown();
}

// =============================================================================
// IsAlive Tests
// =============================================================================

TYPED_TEST(MpsDeviceManagerTypedTest, IsAliveReflectsReportedDeviceCount) {
  auto &manager = this->manager();

  // Arrange
  if constexpr (TypeParam::is_mock) {
    const auto device0 = makeDevice(0xAA);
    const auto device1 = makeDevice(0xBB);
    this->adapter().expectGetDeviceCount(2);
    this->adapter().expectGetDevices({{0, device0}, {1, device1}});
    this->adapter().expectDetectArchitectures({
        {base::DeviceHandle{0}, architecture::Architecture::MpsM3},
        {base::DeviceHandle{1}, architecture::Architecture::MpsM4},
    });
    this->adapter().expectReleaseDevices({device0, device1});
  }

  // Act
  manager.configure(makeConfig(this->getOps()));

  const std::size_t count = manager.getDeviceCountForTest();
  if (const char *expected_env = std::getenv(ORTEAF_MPS_ENV_COUNT);
      expected_env && std::stoi(expected_env) >= 0) {
    EXPECT_EQ(count, static_cast<std::size_t>(std::stoi(expected_env)));
  }

  // Assert: All devices are alive
  for (std::uint32_t index = 0; index < static_cast<std::uint32_t>(count);
       ++index) {
    const auto id = base::DeviceHandle{index};
    EXPECT_TRUE(manager.isAliveForTest(id))
        << "Device " << index << " should be alive";
  }

  // Assert: Out-of-range ID is not alive
  const auto invalid = base::DeviceHandle{static_cast<std::uint32_t>(count)};
  EXPECT_FALSE(manager.isAliveForTest(invalid));

  // Assert: After shutdown, none are alive
  manager.shutdown();
  for (std::uint32_t index = 0; index < static_cast<std::uint32_t>(count);
       ++index) {
    const auto id = base::DeviceHandle{index};
    EXPECT_FALSE(manager.isAliveForTest(id))
        << "Device " << index << " should be inactive after shutdown";
  }
}

TYPED_TEST(MpsDeviceManagerTypedTest, DeviceNotAliveThrowsOnAccess) {
  if constexpr (!TypeParam::is_mock) {
    GTEST_SKIP() << "Mock-only test";
  }
  auto &manager = this->manager();

  // Arrange: Device returns null pointer
  this->adapter().expectGetDeviceCount(1);
  this->adapter().expectGetDevices({{0, nullptr}});
  this->adapter().expectDetectArchitectures({});

  // Act
  manager.configure(makeConfig(this->getOps()));

  // Assert
  EXPECT_EQ(manager.getDeviceCountForTest(), 1u);
  EXPECT_FALSE(manager.isAliveForTest(base::DeviceHandle{0}));

  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.acquire(base::DeviceHandle{0}); });
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.getArch(base::DeviceHandle{0}); });

  // Cleanup
  manager.shutdown();
}

// =============================================================================
// Reinitialization Tests
// =============================================================================

TYPED_TEST(MpsDeviceManagerTypedTest, ReinitializeReleasesPreviousDevices) {
  auto &manager = this->manager();

  // Arrange: First initialization
  const auto first0 = makeDevice(0x301);
  const auto first1 = makeDevice(0x302);
  const auto second0 = makeDevice(0x401);
  const auto second1 = makeDevice(0x402);

  this->adapter().expectGetDeviceCount(2);
  this->adapter().expectGetDevices({{0, first0}, {1, first1}});
  this->adapter().expectDetectArchitectures({
      {base::DeviceHandle{0}, architecture::Architecture::MpsM3},
      {base::DeviceHandle{1}, architecture::Architecture::MpsM4},
  });

  manager.configure(makeConfig(this->getOps()));
  const auto initial_count = manager.getDeviceCountForTest();
  if (initial_count == 0u) {
    manager.shutdown();
    GTEST_SKIP() << "No MPS devices available";
  }

  if constexpr (TypeParam::is_mock) {
    const auto device = manager.acquire(base::DeviceHandle{0});
    auto *resource = device.payloadPtr();
    ASSERT_NE(resource, nullptr);
    EXPECT_EQ(resource->device, first0);
  }

  // Act: Reinitialize with different devices
  this->adapter().expectReleaseDevices({first0, first1});
  this->adapter().expectGetDeviceCount(2);
  this->adapter().expectGetDevices({{0, second0}, {1, second1}});
  this->adapter().expectDetectArchitectures({
      {base::DeviceHandle{0}, architecture::Architecture::MpsM4},
      {base::DeviceHandle{1}, architecture::Architecture::MpsM3},
  });

  manager.configure(makeConfig(this->getOps()));

  // Assert
  const auto reinit_count = manager.getDeviceCountForTest();
  EXPECT_EQ(reinit_count, initial_count);
  if constexpr (TypeParam::is_mock) {
    const auto device = manager.acquire(base::DeviceHandle{0});
    auto *resource = device.payloadPtr();
    ASSERT_NE(resource, nullptr);
    EXPECT_EQ(resource->device, second0);
    EXPECT_NE(second0, first0);
  }

  // Cleanup
  this->adapter().expectReleaseDevices({second0, second1});
  manager.shutdown();
}

// =============================================================================
// Shutdown Tests
// =============================================================================

TYPED_TEST(MpsDeviceManagerTypedTest, ShutdownClearsDeviceState) {
  auto &manager = this->manager();

  // Arrange
  if constexpr (TypeParam::is_mock) {
    const auto device0 = makeDevice(0x111);
    const auto device1 = makeDevice(0x222);
    this->adapter().expectGetDeviceCount(2);
    this->adapter().expectGetDevices({{0, device0}, {1, device1}});
    this->adapter().expectDetectArchitectures({
        {base::DeviceHandle{0}, architecture::Architecture::MpsM3},
        {base::DeviceHandle{1}, architecture::Architecture::MpsM4},
    });
    this->adapter().expectReleaseDevices({device0, device1});
  }

  manager.configure(makeConfig(this->getOps()));
  const auto count = manager.getDeviceCountForTest();
  if (count == 0u) {
    GTEST_SKIP() << "No MPS devices available";
  }

  // Act
  manager.shutdown();

  // Assert: All state cleared
  EXPECT_EQ(manager.getDeviceCountForTest(), 0u);
  EXPECT_FALSE(manager.isInitializedForTest());
  EXPECT_EQ(manager.payloadPoolSizeForTest(), 0u);
  for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(count); ++i) {
    const auto id = base::DeviceHandle{i};
    EXPECT_FALSE(manager.isAliveForTest(id));
  }
}

TYPED_TEST(MpsDeviceManagerTypedTest, ShutdownThrowsWhenActiveLeaseExists) {
  auto &manager = this->manager();

  if constexpr (TypeParam::is_mock) {
    const auto device0 = makeDevice(0x900);
    this->adapter().expectGetDeviceCount(1);
    this->adapter().expectGetDevices({{0, device0}});
    this->adapter().expectDetectArchitectures({
        {base::DeviceHandle{0}, architecture::Architecture::MpsM3},
    });
    this->adapter().expectReleaseDevices({device0});
  }

  manager.configure(makeConfig(this->getOps()));
  if (manager.getDeviceCountForTest() == 0u) {
    GTEST_SKIP() << "No MPS devices available";
  }

  auto lease = manager.acquire(base::DeviceHandle{0});
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { manager.shutdown(); });

  lease.release();
  manager.shutdown();
  EXPECT_FALSE(manager.isInitializedForTest());
}

TYPED_TEST(MpsDeviceManagerTypedTest, ShutdownWithoutInitializeIsNoOp) {
  auto &manager = this->manager();

  // Act
  manager.shutdown();

  // Assert
  EXPECT_FALSE(manager.isInitializedForTest());
  EXPECT_EQ(manager.payloadPoolSizeForTest(), 0u);
}

TYPED_TEST(MpsDeviceManagerTypedTest, MultipleShutdownsAreIdempotent) {
  auto &manager = this->manager();

  // Arrange
  const auto device0 = makeDevice(0x830);
  this->adapter().expectGetDeviceCount(1);
  this->adapter().expectGetDevices({{0, device0}});
  this->adapter().expectDetectArchitectures({
      {base::DeviceHandle{0}, architecture::Architecture::MpsM3},
  });
  this->adapter().expectReleaseDevices({device0});

  manager.configure(makeConfig(this->getOps()));
  EXPECT_TRUE(manager.isInitializedForTest());

  // Act & Assert: Multiple shutdowns are safe
  manager.shutdown();
  EXPECT_FALSE(manager.isInitializedForTest());

  manager.shutdown();
  EXPECT_FALSE(manager.isInitializedForTest());
}

// =============================================================================
// Child Manager Configuration Tests
// =============================================================================

TYPED_TEST(MpsDeviceManagerTypedTest,
           CommandQueueManagersInitializedWithConfiguredCapacity) {
  auto &manager = this->manager();
  constexpr std::size_t kCapacity = 2;

  // Arrange
  auto config = makeConfig(this->getOps());
  config.command_queue_config.payload_capacity = kCapacity;
  config.command_queue_config.control_block_capacity = kCapacity;

  const auto device0 = makeDevice(0x500);
  const auto device1 = makeDevice(0x600);
  this->adapter().expectGetDeviceCount(2);
  this->adapter().expectGetDevices({{0, device0}, {1, device1}});
  this->adapter().expectDetectArchitectures({
      {base::DeviceHandle{0}, architecture::Architecture::MpsM3},
      {base::DeviceHandle{1}, architecture::Architecture::MpsM4},
  });
  this->adapter().expectCreateCommandQueues(
      {makeQueue(0x900), makeQueue(0x901)}, ::testing::Eq(device0));
  this->adapter().expectCreateCommandQueues(
      {makeQueue(0x902), makeQueue(0x903)}, ::testing::Eq(device1));
  this->adapter().expectReleaseDevices({device0, device1});
  this->adapter().expectDestroyCommandQueues(
      {makeQueue(0x900), makeQueue(0x901), makeQueue(0x902), makeQueue(0x903)});

  // Act
  manager.configure(config);
  const auto count = manager.getDeviceCountForTest();
  if (count == 0u) {
    GTEST_SKIP() << "No MPS devices available";
  }

  // Assert: CommandQueueManagers have configured capacity
  for (std::uint32_t index = 0; index < static_cast<std::uint32_t>(count);
       ++index) {
    const auto id = base::DeviceHandle{index};
    const auto device = manager.acquire(id);
    auto *resource = device.payloadPtr();
    ASSERT_NE(resource, nullptr);
    EXPECT_EQ(resource->command_queue_manager.payloadPoolSizeForTest(),
              kCapacity);
  }

  // Cleanup
  manager.shutdown();
}

TYPED_TEST(MpsDeviceManagerTypedTest,
           HeapManagersInitializedWithConfiguredCapacity) {
  auto &manager = this->manager();
  constexpr std::size_t kCapacity = 4;

  // Arrange
  auto config = makeConfig(this->getOps());
  config.heap_initial_capacity = kCapacity;

  const auto device0 = makeDevice(0x700);
  this->adapter().expectGetDeviceCount(1);
  this->adapter().expectGetDevices({{0, device0}});
  this->adapter().expectDetectArchitectures({
      {base::DeviceHandle{0}, architecture::Architecture::MpsM3},
  });
  this->adapter().expectReleaseDevices({device0});

  // Act
  manager.configure(config);
  const auto count = manager.getDeviceCountForTest();
  if (count == 0u) {
    GTEST_SKIP() << "No MPS devices available";
  }

  // Assert: HeapManagers initialized with configured capacity
  for (std::uint32_t index = 0; index < static_cast<std::uint32_t>(count);
       ++index) {
    const auto handle = base::DeviceHandle{index};
    const auto device = manager.acquire(handle);
    auto *resource = device.payloadPtr();
    ASSERT_NE(resource, nullptr);
    EXPECT_EQ(resource->heap_manager.payloadPoolSizeForTest(), kCapacity);
  }

  // Cleanup
  manager.shutdown();
}

TYPED_TEST(MpsDeviceManagerTypedTest,
           LibraryManagersInitializedWithConfiguredCapacity) {
  auto &manager = this->manager();
  constexpr std::size_t kCapacity = 2;

  // Arrange
  auto config = makeConfig(this->getOps());
  config.library_initial_capacity = kCapacity;

  const auto device0 = makeDevice(0x750);
  this->adapter().expectGetDeviceCount(1);
  this->adapter().expectGetDevices({{0, device0}});
  this->adapter().expectDetectArchitectures({
      {base::DeviceHandle{0}, architecture::Architecture::MpsM3},
  });
  this->adapter().expectReleaseDevices({device0});

  // Act
  manager.configure(config);
  const auto count = manager.getDeviceCountForTest();
  if (count == 0u) {
    GTEST_SKIP() << "No MPS devices available";
  }

  // Assert: LibraryManagers initialized with configured capacity
  for (std::uint32_t index = 0; index < static_cast<std::uint32_t>(count);
       ++index) {
    const auto id = base::DeviceHandle{index};
    const auto device = manager.acquire(id);
    auto *resource = device.payloadPtr();
    ASSERT_NE(resource, nullptr);
    EXPECT_EQ(resource->library_manager.payloadPoolSizeForTest(), kCapacity);
  }

  // Cleanup
  manager.shutdown();
}

// =============================================================================
// Pool Access Tests
// =============================================================================

TYPED_TEST(MpsDeviceManagerTypedTest, EventPoolAccessSucceeds) {
  auto &manager = this->manager();

  // Arrange
  const auto device0 = makeDevice(0x800);
  this->adapter().expectGetDeviceCount(1);
  this->adapter().expectGetDevices({{0, device0}});
  this->adapter().expectDetectArchitectures({
      {base::DeviceHandle{0}, architecture::Architecture::MpsM3},
  });
  this->adapter().expectReleaseDevices({device0});

  manager.configure(makeConfig(this->getOps()));
  const auto count = manager.getDeviceCountForTest();
  if (count == 0u) {
    GTEST_SKIP() << "No MPS devices available";
  }

  // Act & Assert
  auto device = manager.acquire(base::DeviceHandle{0});
  auto *resource = device.payloadPtr();
  ASSERT_NE(resource, nullptr);
  EXPECT_NE(&resource->event_pool, nullptr);
  device.release();

  // Cleanup
  manager.shutdown();
}

TYPED_TEST(MpsDeviceManagerTypedTest, FencePoolAccessSucceeds) {
  auto &manager = this->manager();

  // Arrange
  const auto device0 = makeDevice(0x810);
  this->adapter().expectGetDeviceCount(1);
  this->adapter().expectGetDevices({{0, device0}});
  this->adapter().expectDetectArchitectures({
      {base::DeviceHandle{0}, architecture::Architecture::MpsM3},
  });
  this->adapter().expectReleaseDevices({device0});

  manager.configure(makeConfig(this->getOps()));
  const auto count = manager.getDeviceCountForTest();
  if (count == 0u) {
    GTEST_SKIP() << "No MPS devices available";
  }

  // Act & Assert
  auto device = manager.acquire(base::DeviceHandle{0});
  auto *resource = device.payloadPtr();
  ASSERT_NE(resource, nullptr);
  EXPECT_NE(&resource->fence_pool, nullptr);
  device.release();

  // Cleanup
  manager.shutdown();
}

// =============================================================================
// Direct Access Tests
// =============================================================================

TYPED_TEST(MpsDeviceManagerTypedTest, DirectAccessReturnsValidPointers) {
  auto &manager = this->manager();

  // Arrange
  const auto device0 = makeDevice(0x840);
  this->adapter().expectGetDeviceCount(1);
  this->adapter().expectGetDevices({{0, device0}});
  this->adapter().expectDetectArchitectures({
      {base::DeviceHandle{0}, architecture::Architecture::MpsM3},
  });
  this->adapter().expectReleaseDevices({device0});

  manager.configure(makeConfig(this->getOps()));

  // Act & Assert: All accessors return valid pointers
  auto device = manager.acquire(base::DeviceHandle{0});
  auto *resource = device.payloadPtr();
  ASSERT_NE(resource, nullptr);
  if constexpr (TypeParam::is_mock) {
    EXPECT_EQ(resource->device, device0);
  } else {
    EXPECT_TRUE(device);
  }

  device.release();
  EXPECT_NE(&resource->command_queue_manager, nullptr);
  EXPECT_NE(&resource->heap_manager, nullptr);
  EXPECT_NE(&resource->library_manager, nullptr);
  EXPECT_NE(&resource->event_pool, nullptr);
  EXPECT_NE(&resource->fence_pool, nullptr);

  // Cleanup
  manager.shutdown();
}

// =============================================================================
// Lease Copy/Move Tests with WeakCount Verification
// =============================================================================

#if ORTEAF_ENABLE_TEST
TYPED_TEST(MpsDeviceManagerTypedTest, LeaseCopyIncrementsWeakCount) {
  auto &manager = this->manager();

  // Arrange
  const auto device0 = makeDevice(0xF00);
  this->adapter().expectGetDeviceCount(1);
  this->adapter().expectGetDevices({{0, device0}});
  this->adapter().expectDetectArchitectures({
      {base::DeviceHandle{0}, architecture::Architecture::MpsM3},
  });
  this->adapter().expectReleaseDevices({device0});

  manager.configure(makeConfig(this->getOps()));
  const auto count = manager.getDeviceCountForTest();
  if (count == 0u) {
    GTEST_SKIP() << "No MPS devices available";
  }

  // Act
  auto lease1 = manager.acquire(base::DeviceHandle{0});
  EXPECT_EQ(lease1.weakCount(), 1u);

  auto lease2 = lease1; // Copy

  // Assert: Both valid, weak count incremented
  EXPECT_TRUE(lease1);
  EXPECT_TRUE(lease2);
  EXPECT_EQ(lease1.weakCount(), 2u);
  EXPECT_EQ(lease2.weakCount(), 2u);

  // Cleanup
  lease1.release();
  EXPECT_EQ(lease2.weakCount(), 1u);
  lease2.release();

  manager.shutdown();
}

TYPED_TEST(MpsDeviceManagerTypedTest, LeaseCopyAssignmentIncrementsWeakCount) {
  auto &manager = this->manager();

  // Arrange
  const auto device0 = makeDevice(0xF10);
  this->adapter().expectGetDeviceCount(1);
  this->adapter().expectGetDevices({{0, device0}});
  this->adapter().expectDetectArchitectures({
      {base::DeviceHandle{0}, architecture::Architecture::MpsM3},
  });
  this->adapter().expectReleaseDevices({device0});

  manager.configure(makeConfig(this->getOps()));
  const auto count = manager.getDeviceCountForTest();
  if (count == 0u) {
    GTEST_SKIP() << "No MPS devices available";
  }

  auto lease1 = manager.acquire(base::DeviceHandle{0});
  decltype(lease1) lease2;

  // Act
  lease2 = lease1; // Copy assignment

  // Assert
  EXPECT_EQ(lease1.weakCount(), 2u);
  EXPECT_EQ(lease2.weakCount(), 2u);

  // Cleanup
  lease1.release();
  lease2.release();

  manager.shutdown();
}

TYPED_TEST(MpsDeviceManagerTypedTest, LeaseMoveDoesNotChangeWeakCount) {
  auto &manager = this->manager();

  // Arrange
  const auto device0 = makeDevice(0xF20);
  this->adapter().expectGetDeviceCount(1);
  this->adapter().expectGetDevices({{0, device0}});
  this->adapter().expectDetectArchitectures({
      {base::DeviceHandle{0}, architecture::Architecture::MpsM3},
  });
  this->adapter().expectReleaseDevices({device0});

  manager.configure(makeConfig(this->getOps()));
  const auto count = manager.getDeviceCountForTest();
  if (count == 0u) {
    GTEST_SKIP() << "No MPS devices available";
  }

  auto lease1 = manager.acquire(base::DeviceHandle{0});
  EXPECT_EQ(lease1.weakCount(), 1u);

  // Act
  auto lease2 = std::move(lease1); // Move

  // Assert: Source invalid, target valid, weak count unchanged
  EXPECT_FALSE(lease1);
  EXPECT_TRUE(lease2);
  EXPECT_EQ(lease2.weakCount(), 1u);

  // Cleanup
  lease2.release();

  manager.shutdown();
}

TYPED_TEST(MpsDeviceManagerTypedTest,
           LeaseMoveAssignmentDoesNotChangeWeakCount) {
  auto &manager = this->manager();

  // Arrange
  const auto device0 = makeDevice(0xF30);
  this->adapter().expectGetDeviceCount(1);
  this->adapter().expectGetDevices({{0, device0}});
  this->adapter().expectDetectArchitectures({
      {base::DeviceHandle{0}, architecture::Architecture::MpsM3},
  });
  this->adapter().expectReleaseDevices({device0});

  manager.configure(makeConfig(this->getOps()));
  const auto count = manager.getDeviceCountForTest();
  if (count == 0u) {
    GTEST_SKIP() << "No MPS devices available";
  }

  auto lease1 = manager.acquire(base::DeviceHandle{0});
  decltype(lease1) lease2;

  // Act
  lease2 = std::move(lease1); // Move assignment

  // Assert
  EXPECT_FALSE(lease1);
  EXPECT_TRUE(lease2);
  EXPECT_EQ(lease2.weakCount(), 1u);

  // Cleanup
  lease2.release();

  manager.shutdown();
}
#endif

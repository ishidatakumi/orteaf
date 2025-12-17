#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <system_error>
#include <vector>

#include <orteaf/internal/architecture/architecture.h>
#include <orteaf/internal/diagnostics/error/error.h>
#include <orteaf/internal/runtime/mps/manager/mps_device_manager.h>
#include <tests/internal/runtime/mps/manager/testing/backend_ops_provider.h>
#include <tests/internal/runtime/mps/manager/testing/manager_test_fixture.h>
#include <tests/internal/testing/error_assert.h>

namespace architecture = orteaf::internal::architecture;
namespace base = orteaf::internal::base;
namespace diag_error = orteaf::internal::diagnostics::error;
namespace mps_rt = orteaf::internal::runtime::mps::manager;
namespace mps_wrapper = orteaf::internal::runtime::mps::platform::wrapper;
namespace testing_mps = orteaf::tests::runtime::mps::testing;

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
  EXPECT_FALSE(manager.isAlive(base::DeviceHandle{0}));
  ExpectError(diag_error::OrteafErrc::InvalidState, [&] {
    (void)manager.commandQueueManager(base::DeviceHandle{0});
  });
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.heapManager(base::DeviceHandle{0}); });
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.libraryManager(base::DeviceHandle{0}); });
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.eventPool(base::DeviceHandle{0}); });
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.fencePool(base::DeviceHandle{0}); });
  EXPECT_FALSE(manager.isAlive(base::DeviceHandle{0}));
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
  manager.initialize(this->getOps());

  // Assert
  EXPECT_TRUE(manager.isInitializedForTest());
  EXPECT_EQ(manager.capacity(), manager.getDeviceCount());
  if (expected_count >= 0) {
    EXPECT_EQ(manager.getDeviceCount(),
              static_cast<std::size_t>(expected_count));
  }

  // Cleanup
  manager.shutdown();
  EXPECT_FALSE(manager.isInitializedForTest());
  EXPECT_EQ(manager.capacity(), 0u);
}

TYPED_TEST(MpsDeviceManagerTypedTest, InitializeWithNullOpsThrows) {
  auto &manager = this->manager();

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { manager.initialize(nullptr); });
}

TYPED_TEST(MpsDeviceManagerTypedTest, InitializeWithZeroDevicesSucceeds) {
  if constexpr (!TypeParam::is_mock) {
    GTEST_SKIP() << "Mock-only test";
  }
  auto &manager = this->manager();

  // Arrange
  this->adapter().expectGetDeviceCount(0);

  // Act
  manager.initialize(this->getOps());

  // Assert
  EXPECT_TRUE(manager.isInitializedForTest());
  EXPECT_EQ(manager.capacity(), 0u);
  EXPECT_EQ(manager.getDeviceCount(), 0u);
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
  manager.initialize(this->getOps());
  const auto count = manager.getDeviceCount();
  if (expected_count >= 0) {
    EXPECT_EQ(count, static_cast<std::size_t>(expected_count));
  }
  if (count == 0u) {
    GTEST_SKIP() << "No MPS devices available";
  }

  // Assert: Verify each device
  for (std::uint32_t idx = 0; idx < count; ++idx) {
    const auto device = manager.acquire(base::DeviceHandle{idx});
    const auto &snapshot = manager.controlBlockForTest(idx);
    EXPECT_EQ(snapshot.payload().device != nullptr, static_cast<bool>(device));
    EXPECT_EQ(snapshot.isAlive(), static_cast<bool>(device));
    if constexpr (TypeParam::is_mock) {
      EXPECT_EQ(device.pointer(), expected_handles[idx]);
      const auto expected_arch = (idx == 0) ? architecture::Architecture::MpsM3
                                            : architecture::Architecture::MpsM4;
      EXPECT_EQ(snapshot.payload().arch, expected_arch);
    } else {
      EXPECT_TRUE(device);
      if (expected_count >= 0 && idx == 0) {
        EXPECT_NE(snapshot.payload().arch,
                  architecture::Architecture::MpsGeneric);
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
  manager.initialize(this->getOps());
  const auto count = manager.getDeviceCount();
  if (count == 0u) {
    GTEST_SKIP() << "No MPS devices available";
  }

  // Assert: Verify architecture for each device
  for (std::uint32_t idx = 0; idx < count; ++idx) {
    const auto arch = manager.getArch(base::DeviceHandle{idx});
    const auto &snapshot = manager.controlBlockForTest(idx);
    if constexpr (TypeParam::is_mock) {
      const auto expected_arch = (idx == 0) ? architecture::Architecture::MpsM4
                                            : architecture::Architecture::MpsM3;
      EXPECT_EQ(arch, expected_arch);
      EXPECT_EQ(snapshot.payload().arch, expected_arch);
      EXPECT_TRUE(snapshot.payload().device != nullptr);
      EXPECT_TRUE(snapshot.isAlive());
    } else if (expected_arch_env && *expected_arch_env != '\0' && idx == 0) {
      EXPECT_STREQ(expected_arch_env, architecture::idOf(arch).data());
      EXPECT_STREQ(expected_arch_env,
                   architecture::idOf(snapshot.payload().arch).data());
    } else {
      EXPECT_FALSE(architecture::idOf(arch).empty());
      EXPECT_FALSE(architecture::idOf(snapshot.payload().arch).empty());
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

  manager.initialize(this->getOps());
  const auto invalid = base::DeviceHandle{
      static_cast<std::uint32_t>(manager.getDeviceCount() + 1)};

  // Act & Assert: Invalid ID throws
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { (void)manager.acquire(invalid); });
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { (void)manager.getArch(invalid); });
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { (void)manager.commandQueueManager(invalid); });
  EXPECT_FALSE(manager.isAlive(invalid));

  // Cleanup
  manager.shutdown();
}

TYPED_TEST(MpsDeviceManagerTypedTest, InvalidDeviceIdRejectsManagerAccess) {
  auto &manager = this->manager();

  // Arrange
  const auto device0 = makeDevice(0x820);
  this->adapter().expectGetDeviceCount(1);
  this->adapter().expectGetDevices({{0, device0}});
  this->adapter().expectDetectArchitectures({
      {base::DeviceHandle{0}, architecture::Architecture::MpsM3},
  });
  this->adapter().expectReleaseDevices({device0});

  manager.initialize(this->getOps());
  const auto invalid = base::DeviceHandle{
      static_cast<std::uint32_t>(manager.getDeviceCount() + 1)};

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { (void)manager.heapManager(invalid); });
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { (void)manager.libraryManager(invalid); });
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { (void)manager.eventPool(invalid); });
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { (void)manager.fencePool(invalid); });

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
  manager.initialize(this->getOps());

  const std::size_t count = manager.getDeviceCount();
  if (const char *expected_env = std::getenv(ORTEAF_MPS_ENV_COUNT);
      expected_env && std::stoi(expected_env) >= 0) {
    EXPECT_EQ(count, static_cast<std::size_t>(std::stoi(expected_env)));
  }

  // Assert: All devices are alive
  for (std::uint32_t index = 0; index < static_cast<std::uint32_t>(count);
       ++index) {
    const auto id = base::DeviceHandle{index};
    const auto &snapshot = manager.controlBlockForTest(index);
    EXPECT_TRUE(manager.isAlive(id))
        << "Device " << index << " should be alive";
    EXPECT_TRUE(snapshot.isAlive());
  }

  // Assert: Out-of-range ID is not alive
  const auto invalid = base::DeviceHandle{static_cast<std::uint32_t>(count)};
  EXPECT_FALSE(manager.isAlive(invalid));

  // Assert: After shutdown, none are alive
  manager.shutdown();
  for (std::uint32_t index = 0; index < static_cast<std::uint32_t>(count);
       ++index) {
    const auto id = base::DeviceHandle{index};
    EXPECT_FALSE(manager.isAlive(id))
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
  manager.initialize(this->getOps());

  // Assert
  EXPECT_EQ(manager.getDeviceCount(), 1u);
  EXPECT_FALSE(manager.isAlive(base::DeviceHandle{0}));

  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.acquire(base::DeviceHandle{0}); });
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.getArch(base::DeviceHandle{0}); });
  ExpectError(diag_error::OrteafErrc::InvalidState, [&] {
    (void)manager.commandQueueManager(base::DeviceHandle{0});
  });
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.heapManager(base::DeviceHandle{0}); });
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.libraryManager(base::DeviceHandle{0}); });
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.eventPool(base::DeviceHandle{0}); });
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.fencePool(base::DeviceHandle{0}); });

  const auto &snapshot = manager.controlBlockForTest(0);
  // Note: RawControlBlock::isAlive() always returns true (no lifecycle
  // tracking) Check isAlive() or payload directly instead
  EXPECT_FALSE(snapshot.isAlive());
  EXPECT_FALSE(snapshot.payload().device != nullptr);

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

  manager.initialize(this->getOps());
  const auto initial_count = manager.getDeviceCount();
  if (initial_count == 0u) {
    manager.shutdown();
    GTEST_SKIP() << "No MPS devices available";
  }

  if constexpr (TypeParam::is_mock) {
    const auto device = manager.acquire(base::DeviceHandle{0});
    EXPECT_EQ(device.pointer(), first0);
  }

  // Act: Reinitialize with different devices
  this->adapter().expectReleaseDevices({first0, first1});
  this->adapter().expectGetDeviceCount(2);
  this->adapter().expectGetDevices({{0, second0}, {1, second1}});
  this->adapter().expectDetectArchitectures({
      {base::DeviceHandle{0}, architecture::Architecture::MpsM4},
      {base::DeviceHandle{1}, architecture::Architecture::MpsM3},
  });

  manager.initialize(this->getOps());

  // Assert
  const auto reinit_count = manager.getDeviceCount();
  EXPECT_EQ(reinit_count, initial_count);
  if constexpr (TypeParam::is_mock) {
    const auto device = manager.acquire(base::DeviceHandle{0});
    EXPECT_EQ(device.pointer(), second0);
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

  manager.initialize(this->getOps());
  const auto count = manager.getDeviceCount();
  if (count == 0u) {
    GTEST_SKIP() << "No MPS devices available";
  }

  // Act
  manager.shutdown();

  // Assert: All state cleared
  EXPECT_EQ(manager.getDeviceCount(), 0u);
  EXPECT_FALSE(manager.isInitializedForTest());
  EXPECT_EQ(manager.capacity(), 0u);
  for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(count); ++i) {
    const auto id = base::DeviceHandle{i};
    EXPECT_FALSE(manager.isAlive(id));
    ExpectError(diag_error::OrteafErrc::InvalidState,
                [&] { (void)manager.commandQueueManager(id); });
  }
}

TYPED_TEST(MpsDeviceManagerTypedTest, ShutdownWithoutInitializeIsNoOp) {
  auto &manager = this->manager();

  // Act
  manager.shutdown();

  // Assert
  EXPECT_FALSE(manager.isInitializedForTest());
  EXPECT_EQ(manager.capacity(), 0u);
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

  manager.initialize(this->getOps());
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
  manager.setCommandQueueInitialCapacity(kCapacity);

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
  manager.initialize(this->getOps());
  const auto count = manager.getDeviceCount();
  if (count == 0u) {
    GTEST_SKIP() << "No MPS devices available";
  }

  // Assert: CommandQueueManagers have configured capacity
  for (std::uint32_t index = 0; index < static_cast<std::uint32_t>(count);
       ++index) {
    const auto id = base::DeviceHandle{index};
    auto *queue_manager = manager.commandQueueManager(id);
    EXPECT_NE(queue_manager, nullptr);
    EXPECT_EQ(queue_manager->capacity(), kCapacity);
  }

  // Cleanup
  manager.shutdown();
}

TYPED_TEST(MpsDeviceManagerTypedTest,
           HeapManagersInitializedWithConfiguredCapacity) {
  auto &manager = this->manager();
  constexpr std::size_t kCapacity = 4;

  // Arrange
  manager.setHeapInitialCapacity(kCapacity);

  const auto device0 = makeDevice(0x700);
  this->adapter().expectGetDeviceCount(1);
  this->adapter().expectGetDevices({{0, device0}});
  this->adapter().expectDetectArchitectures({
      {base::DeviceHandle{0}, architecture::Architecture::MpsM3},
  });
  this->adapter().expectReleaseDevices({device0});

  // Act
  manager.initialize(this->getOps());
  const auto count = manager.getDeviceCount();
  if (count == 0u) {
    GTEST_SKIP() << "No MPS devices available";
  }

  // Assert: HeapManagers initialized with configured capacity
  for (std::uint32_t index = 0; index < static_cast<std::uint32_t>(count);
       ++index) {
    const auto id = base::DeviceHandle{index};
    auto *heap_manager = manager.heapManager(id);
    EXPECT_NE(heap_manager, nullptr);
    EXPECT_EQ(heap_manager->capacity(), kCapacity);
  }

  // Cleanup
  manager.shutdown();
}

TYPED_TEST(MpsDeviceManagerTypedTest,
           LibraryManagersInitializedWithConfiguredCapacity) {
  auto &manager = this->manager();
  constexpr std::size_t kCapacity = 2;

  // Arrange
  manager.setLibraryInitialCapacity(kCapacity);

  const auto device0 = makeDevice(0x750);
  this->adapter().expectGetDeviceCount(1);
  this->adapter().expectGetDevices({{0, device0}});
  this->adapter().expectDetectArchitectures({
      {base::DeviceHandle{0}, architecture::Architecture::MpsM3},
  });
  this->adapter().expectReleaseDevices({device0});

  // Act
  manager.initialize(this->getOps());
  const auto count = manager.getDeviceCount();
  if (count == 0u) {
    GTEST_SKIP() << "No MPS devices available";
  }

  // Assert: LibraryManagers initialized with configured capacity
  for (std::uint32_t index = 0; index < static_cast<std::uint32_t>(count);
       ++index) {
    const auto id = base::DeviceHandle{index};
    auto *library_manager = manager.libraryManager(id);
    EXPECT_NE(library_manager, nullptr);
    EXPECT_EQ(library_manager->capacity(), kCapacity);
  }

  // Cleanup
  manager.shutdown();
}

TYPED_TEST(MpsDeviceManagerTypedTest, CapacityGettersReturnConfiguredValues) {
  auto &manager = this->manager();

  // Assert: Default values
  EXPECT_EQ(manager.commandQueueInitialCapacity(), 0u);
  EXPECT_EQ(manager.heapInitialCapacity(), 0u);
  EXPECT_EQ(manager.libraryInitialCapacity(), 0u);

  // Act
  manager.setCommandQueueInitialCapacity(5);
  manager.setHeapInitialCapacity(10);
  manager.setLibraryInitialCapacity(15);

  // Assert: Updated values
  EXPECT_EQ(manager.commandQueueInitialCapacity(), 5u);
  EXPECT_EQ(manager.heapInitialCapacity(), 10u);
  EXPECT_EQ(manager.libraryInitialCapacity(), 15u);
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

  manager.initialize(this->getOps());
  const auto count = manager.getDeviceCount();
  if (count == 0u) {
    GTEST_SKIP() << "No MPS devices available";
  }

  // Act & Assert
  auto *event_pool = manager.eventPool(base::DeviceHandle{0});
  EXPECT_NE(event_pool, nullptr);

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

  manager.initialize(this->getOps());
  const auto count = manager.getDeviceCount();
  if (count == 0u) {
    GTEST_SKIP() << "No MPS devices available";
  }

  // Act & Assert
  auto *fence_pool = manager.fencePool(base::DeviceHandle{0});
  EXPECT_NE(fence_pool, nullptr);

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

  manager.initialize(this->getOps());

  // Act & Assert: All accessors return valid pointers
  const auto device = manager.acquire(base::DeviceHandle{0});
  if constexpr (TypeParam::is_mock) {
    EXPECT_EQ(device.pointer(), device0);
  } else {
    EXPECT_TRUE(device);
  }

  auto *queue_manager = manager.commandQueueManager(base::DeviceHandle{0});
  EXPECT_NE(queue_manager, nullptr);

  auto *heap_manager = manager.heapManager(base::DeviceHandle{0});
  EXPECT_NE(heap_manager, nullptr);

  auto *library_manager = manager.libraryManager(base::DeviceHandle{0});
  EXPECT_NE(library_manager, nullptr);

  auto *event_pool = manager.eventPool(base::DeviceHandle{0});
  EXPECT_NE(event_pool, nullptr);

  auto *fence_pool = manager.fencePool(base::DeviceHandle{0});
  EXPECT_NE(fence_pool, nullptr);

  // Cleanup
  manager.shutdown();
}

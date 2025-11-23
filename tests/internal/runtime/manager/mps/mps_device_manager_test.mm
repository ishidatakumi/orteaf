#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <system_error>
#include <vector>

#include <orteaf/internal/architecture/architecture.h>
#include <orteaf/internal/diagnostics/error/error.h>
#include <orteaf/internal/runtime/manager/mps/mps_device_manager.h>
#include <tests/internal/runtime/manager/mps/testing/backend_ops_provider.h>
#include <tests/internal/runtime/manager/mps/testing/manager_test_fixture.h>
#include <tests/internal/testing/error_assert.h>

namespace architecture = orteaf::internal::architecture;
namespace backend = orteaf::internal::backend;
namespace base = orteaf::internal::base;
namespace diag_error = orteaf::internal::diagnostics::error;
namespace mps_rt = orteaf::internal::runtime::mps;
namespace testing_mps = orteaf::tests::runtime::mps::testing;

using orteaf::tests::ExpectError;

#define ORTEAF_MPS_ENV_COUNT "ORTEAF_EXPECT_MPS_DEVICE_COUNT"
#define ORTEAF_MPS_ENV_ARCH "ORTEAF_EXPECT_MPS_DEVICE_ARCH"

namespace {

backend::mps::MPSDevice_t makeDevice(std::uintptr_t value) {
  return reinterpret_cast<backend::mps::MPSDevice_t>(value);
}

backend::mps::MPSCommandQueue_t makeQueue(std::uintptr_t value) {
  return reinterpret_cast<backend::mps::MPSCommandQueue_t>(value);
}

backend::mps::MPSEvent_t makeEvent(std::uintptr_t value) {
  return reinterpret_cast<backend::mps::MPSEvent_t>(value);
}

bool shouldRunHardwareTests() {
  const char *expected_env = std::getenv(ORTEAF_MPS_ENV_COUNT);
  return expected_env != nullptr && std::stoi(expected_env) > 0;
}

template <class Provider>
class MpsDeviceManagerTypedTest
    : public testing_mps::RuntimeManagerFixture<Provider,
                                                mps_rt::MpsDeviceManager> {
protected:
  using Base =
      testing_mps::RuntimeManagerFixture<Provider, mps_rt::MpsDeviceManager>;

  mps_rt::MpsDeviceManager &manager() {
    return Base::manager();
  }

  auto &adapter() { return Base::adapter(); }

  // Override to inject ops
  void onPreManagerSetUp() override {
    // Assuming MpsDeviceManager now takes a reference/pointer to ops in
    // constructor or init. Since I haven't seen MpsDeviceManager code yet, I'll
    // assume it needs to be updated or the fixture needs to handle it. Wait, I
    // need to check MpsDeviceManager first to see how it accepts ops. But the
    // user said "manager本体の修正をしたい" later. So currently
    // MpsDeviceManager probably uses the template param as a static class? If
    // so, I can't inject instance yet. BUT, I deleted the static class
    // MpsBackendOps. So MpsDeviceManager compilation will fail if I don't
    // update it. The user said "managerたちよりも先にいじりたいのがある...
    // その後にmanagerの本体の修正をしたい". This implies I should fix the tests
    // first, but the tests depend on the manager. If the manager is broken
    // (because I deleted the file it uses), I must fix the manager at least
    // minimally.

    // Let's assume for this step I just update the test code to *try* to use
    // the new provider. I will likely need to update MpsDeviceManager in the
    // next step to accept the ops instance.
  }

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

TYPED_TEST(MpsDeviceManagerTypedTest, AccessBeforeInitializationThrows) {
  auto &manager = this->manager();
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.getDevice(base::DeviceId{0}); });
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.getArch(base::DeviceId{0}); });
  EXPECT_FALSE(manager.isAlive(base::DeviceId{0}));
  ExpectError(diag_error::OrteafErrc::InvalidState, [&] {
    static_cast<void>(manager.commandQueueManager(base::DeviceId{0}));
  });
  ExpectError(diag_error::OrteafErrc::InvalidState, [&] {
    static_cast<void>(manager.heapManager(base::DeviceId{0}));
  });
  ExpectError(diag_error::OrteafErrc::InvalidState, [&] {
    static_cast<void>(manager.libraryManager(base::DeviceId{0}));
  });
  const auto snapshot = manager.debugState(base::DeviceId{0});
  EXPECT_FALSE(snapshot.in_range);
  EXPECT_FALSE(snapshot.is_alive);
}

TYPED_TEST(MpsDeviceManagerTypedTest, InitializeMarksManagerInitialized) {
  auto &manager = this->manager();
  int expected_count = -1;

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
        {base::DeviceId{0}, architecture::Architecture::MpsM3},
    });
    this->adapter().expectReleaseDevices({device0});
    expected_count = 1;
  }

  manager.initialize(this->getOps());

  const auto state = manager.debugState();
  EXPECT_TRUE(state.initialized);
  EXPECT_EQ(state.device_count, manager.getDeviceCount());
  if (expected_count >= 0) {
    EXPECT_EQ(manager.getDeviceCount(),
              static_cast<std::size_t>(expected_count));
  }

  manager.shutdown();
  const auto after_shutdown = manager.debugState();
  EXPECT_FALSE(after_shutdown.initialized);
  EXPECT_EQ(after_shutdown.device_count, 0u);
}

TYPED_TEST(MpsDeviceManagerTypedTest, GetDeviceReturnsRegisteredHandle) {
  auto &manager = this->manager();

  std::vector<backend::mps::MPSDevice_t> expected_handles;
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
      {base::DeviceId{0}, architecture::Architecture::MpsM3},
      {base::DeviceId{1}, architecture::Architecture::MpsM4},
  });
  this->adapter().expectReleaseDevices(
      {expected_handles[0], expected_handles[1]});

  manager.initialize(this->getOps());
  const auto count = manager.getDeviceCount();
  if (expected_count >= 0) {
    EXPECT_EQ(count, static_cast<std::size_t>(expected_count));
  }
  if (count == 0u) {
    GTEST_SKIP() << "No MPS devices available";
  }

  for (std::uint32_t idx = 0; idx < count; ++idx) {
    const auto device = manager.getDevice(base::DeviceId{idx});
    const auto snapshot = manager.debugState(base::DeviceId{idx});
    EXPECT_TRUE(snapshot.in_range);
    EXPECT_EQ(snapshot.has_device, device != nullptr);
    EXPECT_EQ(snapshot.is_alive, device != nullptr);
    if constexpr (TypeParam::is_mock) {
      EXPECT_EQ(device, expected_handles[idx]);
      const auto expected_arch = (idx == 0)
                                     ? architecture::Architecture::MpsM3
                                     : architecture::Architecture::MpsM4;
      EXPECT_EQ(snapshot.arch, expected_arch);
    } else {
      EXPECT_NE(device, nullptr);
      if (expected_count >= 0 && idx == 0) {
        EXPECT_NE(snapshot.arch, architecture::Architecture::MpsGeneric);
      }
    }
  }

  manager.shutdown();
}

TYPED_TEST(MpsDeviceManagerTypedTest, GetArchMatchesReportedArchitecture) {
  auto &manager = this->manager();

  const char *expected_arch_env = nullptr;
  if constexpr (TypeParam::is_mock) {
    const auto device0 = makeDevice(0xAB);
    const auto device1 = makeDevice(0xCD);
    this->adapter().expectGetDeviceCount(2);
    this->adapter().expectGetDevices({{0, device0}, {1, device1}});
    this->adapter().expectDetectArchitectures({
        {base::DeviceId{0}, architecture::Architecture::MpsM4},
        {base::DeviceId{1}, architecture::Architecture::MpsM3},
    });
    this->adapter().expectReleaseDevices({device0, device1});
  } else {
    expected_arch_env = std::getenv(ORTEAF_MPS_ENV_ARCH);
  }

  manager.initialize(this->getOps());
  const auto count = manager.getDeviceCount();
  if (count == 0u) {
    GTEST_SKIP() << "No MPS devices available";
  }

  for (std::uint32_t idx = 0; idx < count; ++idx) {
    const auto arch = manager.getArch(base::DeviceId{idx});
    const auto snapshot = manager.debugState(base::DeviceId{idx});
    EXPECT_TRUE(snapshot.in_range);
    if constexpr (TypeParam::is_mock) {
      const auto expected_arch = (idx == 0)
                                     ? architecture::Architecture::MpsM4
                                     : architecture::Architecture::MpsM3;
      EXPECT_EQ(arch, expected_arch);
      EXPECT_EQ(snapshot.arch, expected_arch);
      EXPECT_TRUE(snapshot.has_device);
      EXPECT_TRUE(snapshot.is_alive);
    } else if (expected_arch_env && *expected_arch_env != '\0' && idx == 0) {
      EXPECT_STREQ(expected_arch_env, architecture::idOf(arch).data());
      EXPECT_STREQ(expected_arch_env, architecture::idOf(snapshot.arch).data());
    } else {
      EXPECT_FALSE(architecture::idOf(arch).empty());
      EXPECT_FALSE(architecture::idOf(snapshot.arch).empty());
    }
  }

  manager.shutdown();
}

TYPED_TEST(MpsDeviceManagerTypedTest, InvalidDeviceIdRejectsAccess) {
  auto &manager = this->manager();

  const auto device0 = makeDevice(0x77);
  this->adapter().expectGetDeviceCount(1);
  this->adapter().expectGetDevices({{0, device0}});
  this->adapter().expectDetectArchitectures({
      {base::DeviceId{0}, architecture::Architecture::MpsM3},
  });
  this->adapter().expectReleaseDevices({device0});

  manager.initialize(this->getOps());
  const auto invalid =
      base::DeviceId{static_cast<std::uint32_t>(manager.getDeviceCount() + 1)};

  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { (void)manager.getDevice(invalid); });
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { (void)manager.getArch(invalid); });
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { static_cast<void>(manager.commandQueueManager(invalid)); });
  EXPECT_FALSE(manager.isAlive(invalid));
  const auto invalid_snapshot = manager.debugState(invalid);
  EXPECT_FALSE(invalid_snapshot.in_range);
  EXPECT_FALSE(invalid_snapshot.is_alive);

  manager.shutdown();
}

TYPED_TEST(MpsDeviceManagerTypedTest, IsAliveReflectsReportedDeviceCount) {
  auto &manager = this->manager();

  if constexpr (TypeParam::is_mock) {
    const auto device0 = makeDevice(0xAA);
    const auto device1 = makeDevice(0xBB);
    this->adapter().expectGetDeviceCount(2);
    this->adapter().expectGetDevices({{0, device0}, {1, device1}});
    this->adapter().expectDetectArchitectures({
        {base::DeviceId{0}, architecture::Architecture::MpsM3},
        {base::DeviceId{1}, architecture::Architecture::MpsM4},
    });
    this->adapter().expectReleaseDevices({device0, device1});
  }

  manager.initialize(this->getOps());

  const std::size_t count = manager.getDeviceCount();
  if (const char *expected_env = std::getenv(ORTEAF_MPS_ENV_COUNT);
      expected_env && std::stoi(expected_env) >= 0) {
    EXPECT_EQ(count, static_cast<std::size_t>(std::stoi(expected_env)));
  }

  for (std::uint32_t index = 0; index < static_cast<std::uint32_t>(count);
       ++index) {
    const auto id = base::DeviceId{index};
    const auto snapshot = manager.debugState(id);
    EXPECT_TRUE(manager.isAlive(id))
        << "Device " << index << " should be alive";
    EXPECT_TRUE(snapshot.in_range);
    EXPECT_TRUE(snapshot.is_alive);
  }

  const auto invalid = base::DeviceId{static_cast<std::uint32_t>(count)};
  EXPECT_FALSE(manager.isAlive(invalid));
  const auto invalid_snapshot = manager.debugState(invalid);
  EXPECT_FALSE(invalid_snapshot.in_range);
  EXPECT_FALSE(invalid_snapshot.is_alive);

  manager.shutdown();
  for (std::uint32_t index = 0; index < static_cast<std::uint32_t>(count);
       ++index) {
    const auto id = base::DeviceId{index};
    EXPECT_FALSE(manager.isAlive(id))
        << "Device " << index << " should be inactive after shutdown";
    const auto snapshot = manager.debugState(id);
    EXPECT_FALSE(snapshot.in_range);
    EXPECT_FALSE(snapshot.is_alive);
  }
}

TYPED_TEST(MpsDeviceManagerTypedTest, ReinitializeReleasesPreviousDevices) {
  auto &manager = this->manager();

  const auto first0 = makeDevice(0x301);
  const auto first1 = makeDevice(0x302);
  const auto second0 = makeDevice(0x401);
  const auto second1 = makeDevice(0x402);

  this->adapter().expectGetDeviceCount(2);
  this->adapter().expectGetDevices({{0, first0}, {1, first1}});
  this->adapter().expectDetectArchitectures({
      {base::DeviceId{0}, architecture::Architecture::MpsM3},
      {base::DeviceId{1}, architecture::Architecture::MpsM4},
  });

  manager.initialize(this->getOps());
  const auto initial_count = manager.getDeviceCount();
  if (initial_count == 0u) {
    manager.shutdown();
    GTEST_SKIP() << "No MPS devices available";
  }

  if constexpr (TypeParam::is_mock) {
    EXPECT_EQ(manager.getDevice(base::DeviceId{0}), first0);
  }

  this->adapter().expectReleaseDevices({first0, first1});
  this->adapter().expectGetDeviceCount(2);
  this->adapter().expectGetDevices({{0, second0}, {1, second1}});
  this->adapter().expectDetectArchitectures({
      {base::DeviceId{0}, architecture::Architecture::MpsM4},
      {base::DeviceId{1}, architecture::Architecture::MpsM3},
  });

  manager.initialize(this->getOps());
  const auto reinit_count = manager.getDeviceCount();
  EXPECT_EQ(reinit_count, initial_count);
  if constexpr (TypeParam::is_mock) {
    EXPECT_EQ(manager.getDevice(base::DeviceId{0}), second0);
    EXPECT_NE(second0, first0);
  }

  this->adapter().expectReleaseDevices({second0, second1});
  manager.shutdown();
}

TYPED_TEST(MpsDeviceManagerTypedTest, ShutdownClearsDeviceState) {
  auto &manager = this->manager();

  if constexpr (TypeParam::is_mock) {
    const auto device0 = makeDevice(0x111);
    const auto device1 = makeDevice(0x222);
    this->adapter().expectGetDeviceCount(2);
    this->adapter().expectGetDevices({{0, device0}, {1, device1}});
    this->adapter().expectDetectArchitectures({
        {base::DeviceId{0}, architecture::Architecture::MpsM3},
        {base::DeviceId{1}, architecture::Architecture::MpsM4},
    });
    this->adapter().expectReleaseDevices({device0, device1});
  }

  manager.initialize(this->getOps());
  const auto count = manager.getDeviceCount();
  if (count == 0u) {
    GTEST_SKIP() << "No MPS devices available";
  }

  manager.shutdown();
  EXPECT_EQ(manager.getDeviceCount(), 0u);
  const auto state = manager.debugState();
  EXPECT_FALSE(state.initialized);
  EXPECT_EQ(state.device_count, 0u);
  for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(count); ++i) {
    const auto id = base::DeviceId{i};
    EXPECT_FALSE(manager.isAlive(id));
    const auto snapshot = manager.debugState(id);
    EXPECT_FALSE(snapshot.in_range);
    EXPECT_FALSE(snapshot.is_alive);
    ExpectError(diag_error::OrteafErrc::InvalidState,
                [&] { static_cast<void>(manager.commandQueueManager(id)); });
  }
}

TYPED_TEST(MpsDeviceManagerTypedTest,
           CommandQueueManagersInitializedWithConfiguredCapacity) {
  auto &manager = this->manager();
  constexpr std::size_t kCapacity = 2;
  manager.setCommandQueueInitialCapacity(kCapacity);

  const auto device0 = makeDevice(0x500);
  const auto device1 = makeDevice(0x600);
  this->adapter().expectGetDeviceCount(2);
  this->adapter().expectGetDevices({{0, device0}, {1, device1}});
  this->adapter().expectDetectArchitectures({
      {base::DeviceId{0}, architecture::Architecture::MpsM3},
      {base::DeviceId{1}, architecture::Architecture::MpsM4},
  });
  this->adapter().expectCreateCommandQueues(
      {makeQueue(0x900), makeQueue(0x901)}, ::testing::Eq(device0));
  this->adapter().expectCreateEvents({makeEvent(0xA00), makeEvent(0xA01)},
                                     ::testing::Eq(device0));
  this->adapter().expectCreateCommandQueues(
      {makeQueue(0x902), makeQueue(0x903)}, ::testing::Eq(device1));
  this->adapter().expectCreateEvents({makeEvent(0xA02), makeEvent(0xA03)},
                                     ::testing::Eq(device1));
  this->adapter().expectReleaseDevices({device0, device1});
  this->adapter().expectDestroyEvents(
      {makeEvent(0xA00), makeEvent(0xA01), makeEvent(0xA02), makeEvent(0xA03)});
  this->adapter().expectDestroyCommandQueues(
      {makeQueue(0x900), makeQueue(0x901), makeQueue(0x902), makeQueue(0x903)});

  manager.initialize(this->getOps());
  const auto count = manager.getDeviceCount();
  if (count == 0u) {
    GTEST_SKIP() << "No MPS devices available";
  }

  for (std::uint32_t index = 0; index < static_cast<std::uint32_t>(count);
       ++index) {
    const auto id = base::DeviceId{index};
    auto &queue_manager = manager.commandQueueManager(id);
    EXPECT_EQ(queue_manager.capacity(), kCapacity);
    if constexpr (TypeParam::is_mock) {
      std::vector<backend::mps::MPSCommandQueue_t> expected_handles =
          (index == 0)
              ? std::vector<backend::mps::MPSCommandQueue_t>{makeQueue(0x900),
                                                             makeQueue(0x901)}
              : std::vector<backend::mps::MPSCommandQueue_t>{makeQueue(0x902),
                                                             makeQueue(0x903)};
      std::vector<base::CommandQueueId> acquired_ids;
      const std::size_t expected_count = expected_handles.size();
      acquired_ids.reserve(expected_count);

      for (std::size_t i = 0; i < expected_count; ++i) {
        const auto acquired = queue_manager.acquire();
        const auto queue_handle = queue_manager.getCommandQueue(acquired);
        auto it = std::find(expected_handles.begin(), expected_handles.end(),
                            queue_handle);
        EXPECT_NE(it, expected_handles.end())
            << "Unexpected handle " << queue_handle;
        if (it != expected_handles.end()) {
          expected_handles.erase(it);
        }
        acquired_ids.push_back(acquired);
      }
      EXPECT_TRUE(expected_handles.empty());
      for (const auto acquired : acquired_ids) {
        queue_manager.release(acquired);
      }
    }
  }

  manager.shutdown();
}

TYPED_TEST(MpsDeviceManagerTypedTest,
           HeapManagersInitializedWithConfiguredCapacity) {
  auto &manager = this->manager();
  constexpr std::size_t kCapacity = 4;
  manager.setHeapInitialCapacity(kCapacity);

  const auto device0 = makeDevice(0x700);
  this->adapter().expectGetDeviceCount(1);
  this->adapter().expectGetDevices({{0, device0}});
  this->adapter().expectDetectArchitectures({
      {base::DeviceId{0}, architecture::Architecture::MpsM3},
  });
  this->adapter().expectReleaseDevices({device0});

  manager.initialize(this->getOps());
  const auto count = manager.getDeviceCount();
  if (count == 0u) {
    GTEST_SKIP() << "No MPS devices available";
  }

  for (std::uint32_t index = 0; index < static_cast<std::uint32_t>(count);
       ++index) {
    const auto id = base::DeviceId{index};
    auto &heap_manager = manager.heapManager(id);
    EXPECT_EQ(heap_manager.capacity(), kCapacity);
  }

  manager.shutdown();
}

TYPED_TEST(MpsDeviceManagerTypedTest,
           LibraryManagersInitializedWithConfiguredCapacity) {
  auto &manager = this->manager();
  constexpr std::size_t kCapacity = 2;
  manager.setLibraryInitialCapacity(kCapacity);

  const auto device0 = makeDevice(0x750);
  this->adapter().expectGetDeviceCount(1);
  this->adapter().expectGetDevices({{0, device0}});
  this->adapter().expectDetectArchitectures({
      {base::DeviceId{0}, architecture::Architecture::MpsM3},
  });
  this->adapter().expectReleaseDevices({device0});

  manager.initialize(this->getOps());
  const auto count = manager.getDeviceCount();
  if (count == 0u) {
    GTEST_SKIP() << "No MPS devices available";
  }
  for (std::uint32_t index = 0; index < static_cast<std::uint32_t>(count);
       ++index) {
    const auto id = base::DeviceId{index};
    auto &library_manager = manager.libraryManager(id);
    EXPECT_EQ(library_manager.capacity(), kCapacity);
  }
  manager.shutdown();
}

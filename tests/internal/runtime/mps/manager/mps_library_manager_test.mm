#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include <orteaf/internal/diagnostics/error/error.h>
#include <orteaf/internal/runtime/mps/manager/mps_compute_pipeline_state_manager.h>
#include <orteaf/internal/runtime/mps/manager/mps_library_manager.h>
#include <tests/internal/runtime/mps/manager/testing/backend_ops_provider.h>
#include <tests/internal/runtime/mps/manager/testing/manager_test_fixture.h>
#include <tests/internal/testing/error_assert.h>

namespace diag_error = orteaf::internal::diagnostics::error;
namespace mps_rt = orteaf::internal::runtime::mps::manager;
namespace mps_wrapper = orteaf::internal::runtime::mps::platform::wrapper;
namespace testing_mps = orteaf::tests::runtime::mps::testing;

using orteaf::tests::ExpectError;

namespace {

mps_wrapper::MPSLibrary_t makeLibrary(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MPSLibrary_t>(value);
}

mps_wrapper::MPSFunction_t makeFunction(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MPSFunction_t>(value);
}

mps_wrapper::MPSComputePipelineState_t makePipeline(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MPSComputePipelineState_t>(value);
}

template <class Provider>
class MpsLibraryManagerTypedTest
    : public testing_mps::RuntimeManagerFixture<Provider,
                                                mps_rt::MpsLibraryManager> {
protected:
  using Base =
      testing_mps::RuntimeManagerFixture<Provider, mps_rt::MpsLibraryManager>;

  mps_rt::MpsLibraryManager &manager() { return Base::manager(); }
  auto &adapter() { return Base::adapter(); }

  void initializeManager(std::size_t capacity = 0) {
    const auto device = this->adapter().device();
    manager().initialize(device, this->getOps(), capacity);
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

TYPED_TEST_SUITE(MpsLibraryManagerTypedTest, ProviderTypes);

TYPED_TEST(MpsLibraryManagerTypedTest, GrowthChunkSizeCanBeAdjusted) {
  auto &manager = this->manager();
  EXPECT_EQ(manager.growthChunkSize(), 1u);
  manager.setGrowthChunkSize(3);
  EXPECT_EQ(manager.growthChunkSize(), 3u);
}

TYPED_TEST(MpsLibraryManagerTypedTest, GrowthChunkSizeRejectsZero) {
  auto &manager = this->manager();
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { manager.setGrowthChunkSize(0); });
}

TYPED_TEST(MpsLibraryManagerTypedTest, AccessBeforeInitializationThrows) {
  auto &manager = this->manager();
  ExpectError(diag_error::OrteafErrc::InvalidState, [&] {
    (void)manager.acquire(mps_rt::LibraryKey::Named("test"));
  });
}

TYPED_TEST(MpsLibraryManagerTypedTest, InitializeRejectsNullDevice) {
  auto &manager = this->manager();
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { manager.initialize(nullptr, this->getOps(), 1); });
}

TYPED_TEST(MpsLibraryManagerTypedTest, InitializeRejectsNullOps) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { manager.initialize(device, nullptr, 1); });
}

TYPED_TEST(MpsLibraryManagerTypedTest, InitializeRejectsCapacityAboveLimit) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
    manager.initialize(device, this->getOps(),
                       std::numeric_limits<std::size_t>::max());
  });
}

TYPED_TEST(MpsLibraryManagerTypedTest, CapacityReflectsConfiguredPool) {
  auto &manager = this->manager();
  this->initializeManager(2);
  // Cache pattern: capacity is 0 after init, grows on demand
  EXPECT_EQ(manager.capacity(), 0u);
}

TYPED_TEST(MpsLibraryManagerTypedTest, GrowthChunkSizeControlsPoolExpansion) {
  if constexpr (!TypeParam::is_mock) {
    GTEST_SKIP() << "Mock-only test";
    return;
  }
  auto &manager = this->manager();
  manager.setGrowthChunkSize(3);
  this->initializeManager();
  const auto lib_handle = makeLibrary(0x800);
  this->adapter().expectCreateLibraries({{"GrowthTest0", lib_handle}});
  this->adapter().expectDestroyLibraries({lib_handle});

  auto first = manager.acquire(mps_rt::LibraryKey::Named("GrowthTest0"));
  // Cache pattern: capacity grows on demand, one at a time
  EXPECT_EQ(manager.capacity(), 1u);
  first.release();
}

TYPED_TEST(MpsLibraryManagerTypedTest, GetOrCreateAllocatesAndCachesLibrary) {
  if constexpr (!TypeParam::is_mock) {
    GTEST_SKIP() << "Mock-only test";
    return;
  }
  auto &manager = this->manager();
  this->initializeManager();
  const auto lib_handle = makeLibrary(0x880);
  this->adapter().expectCreateLibraries({{"Foobar", lib_handle}});
  this->adapter().expectDestroyLibraries({lib_handle});

  auto lease = manager.acquire(mps_rt::LibraryKey::Named("Foobar"));
  EXPECT_EQ(lease.with_resource([](auto &r) { return r; }), lib_handle);
  const auto &snapshot = manager.stateForTest(lease.handle().index);
  EXPECT_TRUE(snapshot.alive);
  EXPECT_EQ(snapshot.use_count, 1u);
  lease.release();
  EXPECT_EQ(snapshot.use_count, 0u);
  // Library is still alive after release (cache pattern)
  EXPECT_TRUE(snapshot.alive);
}

TYPED_TEST(MpsLibraryManagerTypedTest, ReleasedLeaseDoesNotAffectLibrary) {
  if constexpr (!TypeParam::is_mock) {
    GTEST_SKIP() << "Mock-only test";
    return;
  }
  auto &manager = this->manager();
  this->initializeManager();
  const auto lib_handle = makeLibrary(0x890);
  this->adapter().expectCreateLibraries({{"Baz", lib_handle}});
  this->adapter().expectDestroyLibraries({lib_handle});

  auto first = manager.acquire(mps_rt::LibraryKey::Named("Baz"));
  auto second = manager.acquire(mps_rt::LibraryKey::Named("Baz"));
  const auto &snapshot = manager.stateForTest(first.handle().index);
  EXPECT_EQ(snapshot.use_count, 2u);
  first.release();
  EXPECT_EQ(snapshot.use_count, 1u);
  second.release();
  EXPECT_EQ(snapshot.use_count, 0u);
  EXPECT_TRUE(snapshot.alive);
}

TYPED_TEST(MpsLibraryManagerTypedTest, ReleaseIsIdempotent) {
  if constexpr (!TypeParam::is_mock) {
    GTEST_SKIP() << "Mock-only test";
    return;
  }
  auto &manager = this->manager();
  this->initializeManager();
  const auto lib_handle = makeLibrary(0x8A0);
  this->adapter().expectCreateLibraries({{"Qux", lib_handle}});
  this->adapter().expectDestroyLibraries({lib_handle});

  auto lease = manager.acquire(mps_rt::LibraryKey::Named("Qux"));
  manager.release(lease);
  manager.release(lease);
}

TYPED_TEST(MpsLibraryManagerTypedTest,
           PipelineManagerProvidesNestedFunctionManager) {
  auto &manager = this->manager();
  this->initializeManager();
  if constexpr (!TypeParam::is_mock) {
    GTEST_SKIP() << "Mock-only test";
    return;
  }
  const auto library_handle = makeLibrary(0x900);
  this->adapter().expectCreateLibraries({{"TestLib", library_handle}});
  this->adapter().expectDestroyLibraries({library_handle});

  auto library_lease = manager.acquire(mps_rt::LibraryKey::Named("TestLib"));
  auto *pipeline_manager = manager.pipelineManager(library_lease);
  EXPECT_NE(pipeline_manager, nullptr);
  EXPECT_TRUE(pipeline_manager->isInitializedForTest());
  library_lease.release();
}

TYPED_TEST(MpsLibraryManagerTypedTest, PipelineManagerCanBeAccessedByKey) {
  auto &manager = this->manager();
  this->initializeManager();
  if constexpr (!TypeParam::is_mock) {
    GTEST_SKIP() << "Mock-only test";
    return;
  }
  const auto key = mps_rt::LibraryKey::Named("ByKey");
  const auto lib_handle = makeLibrary(0x910);
  this->adapter().expectCreateLibraries({{"ByKey", lib_handle}});
  this->adapter().expectDestroyLibraries({lib_handle});

  auto *pipeline_manager = manager.pipelineManager(key);
  EXPECT_NE(pipeline_manager, nullptr);
  // Library was created for the pipeline manager
  EXPECT_EQ(manager.capacity(), 1u);
}

TYPED_TEST(MpsLibraryManagerTypedTest, LibraryPersistsAfterLeaseRelease) {
  auto &manager = this->manager();
  this->initializeManager();
  if constexpr (!TypeParam::is_mock) {
    GTEST_SKIP() << "Mock-only test";
    return;
  }
  const auto key = mps_rt::LibraryKey::Named("KeepAlive");
  const auto lib_handle = makeLibrary(0x930);
  this->adapter().expectCreateLibraries({{"KeepAlive", lib_handle}});
  this->adapter().expectDestroyLibraries({lib_handle});

  auto library_lease = manager.acquire(key);
  auto *pipeline_manager = manager.pipelineManager(library_lease);
  EXPECT_NE(pipeline_manager, nullptr);

  library_lease.release();
  const auto &snapshot_mid = manager.stateForTest(library_lease.handle().index);
  EXPECT_TRUE(snapshot_mid.alive);
}

TYPED_TEST(MpsLibraryManagerTypedTest,
           PipelineManagerAccessByKeyReusesExistingLibrary) {
  auto &manager = this->manager();
  this->initializeManager();
  if constexpr (!TypeParam::is_mock) {
    GTEST_SKIP() << "Mock-only test";
    return;
  }
  const auto key = mps_rt::LibraryKey::Named("ReuseByKey");
  const auto lib_handle = makeLibrary(0x940);
  this->adapter().expectCreateLibraries({{"ReuseByKey", lib_handle}});
  this->adapter().expectDestroyLibraries({lib_handle});

  auto library_lease = manager.acquire(key);
  auto *pm1 = manager.pipelineManager(key);
  auto *pm2 = manager.pipelineManager(library_lease);

  // Same pipeline manager for same library
  EXPECT_EQ(pm1, pm2);

  library_lease.release();

  // Library is still alive after release
  const auto &snapshot = manager.stateForTest(library_lease.handle().index);
  EXPECT_TRUE(snapshot.alive);
}

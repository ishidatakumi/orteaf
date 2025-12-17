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

mps_wrapper::MpsLibrary_t makeLibrary(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MpsLibrary_t>(value);
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

// =============================================================================
// Configuration Tests
// =============================================================================

TYPED_TEST(MpsLibraryManagerTypedTest, GrowthChunkSizeCanBeAdjusted) {
  auto &manager = this->manager();

  // Assert: Default is 1
  EXPECT_EQ(manager.growthChunkSize(), 1u);

  // Act
  manager.setGrowthChunkSize(3);

  // Assert
  EXPECT_EQ(manager.growthChunkSize(), 3u);
}

TYPED_TEST(MpsLibraryManagerTypedTest, GrowthChunkSizeRejectsZero) {
  auto &manager = this->manager();

  // Act & Assert: Zero is invalid
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { manager.setGrowthChunkSize(0); });
}

// =============================================================================
// Initialization Tests
// =============================================================================

TYPED_TEST(MpsLibraryManagerTypedTest, AccessBeforeInitializationThrows) {
  auto &manager = this->manager();

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidState, [&] {
    (void)manager.acquire(mps_rt::LibraryKey::Named("test"));
  });
}

TYPED_TEST(MpsLibraryManagerTypedTest, InitializeRejectsNullDevice) {
  auto &manager = this->manager();

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { manager.initialize(nullptr, this->getOps(), 1); });
}

TYPED_TEST(MpsLibraryManagerTypedTest, InitializeRejectsNullOps) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { manager.initialize(device, nullptr, 1); });
}

TYPED_TEST(MpsLibraryManagerTypedTest, InitializeRejectsCapacityAboveLimit) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
    manager.initialize(device, this->getOps(),
                       std::numeric_limits<std::size_t>::max());
  });
}

TYPED_TEST(MpsLibraryManagerTypedTest, CapacityReflectsConfiguredPool) {
  auto &manager = this->manager();

  // Act
  this->initializeManager(2);

  // Assert: capacity is 2 after init with capacity=2
  EXPECT_EQ(manager.capacity(), 2u);
}

// =============================================================================
// Acquire/Release Tests
// =============================================================================

TYPED_TEST(MpsLibraryManagerTypedTest, GrowthChunkSizeControlsPoolExpansion) {
  if constexpr (!TypeParam::is_mock) {
    GTEST_SKIP() << "Mock-only test";
    return;
  }
  auto &manager = this->manager();

  // Arrange
  manager.setGrowthChunkSize(3);
  this->initializeManager();
  const auto lib_handle = makeLibrary(0x800);
  this->adapter().expectCreateLibraries({{"GrowthTest0", lib_handle}});
  this->adapter().expectDestroyLibraries({lib_handle});

  // Act
  auto first = manager.acquire(mps_rt::LibraryKey::Named("GrowthTest0"));

  // Assert: capacity grows by growth chunk size (3)
  EXPECT_EQ(manager.capacity(), 3u);

  // Cleanup
  manager.release(first);
}

TYPED_TEST(MpsLibraryManagerTypedTest, GetOrCreateAllocatesAndCachesLibrary) {
  if constexpr (!TypeParam::is_mock) {
    GTEST_SKIP() << "Mock-only test";
    return;
  }
  auto &manager = this->manager();

  // Arrange
  this->initializeManager();
  const auto lib_handle = makeLibrary(0x880);
  this->adapter().expectCreateLibraries({{"Foobar", lib_handle}});
  this->adapter().expectDestroyLibraries({lib_handle});

  // Act
  auto lease = manager.acquire(mps_rt::LibraryKey::Named("Foobar"));

  // Assert
  EXPECT_EQ(lease.pointer(), lib_handle);
  const auto &snapshot = manager.controlBlockForTest(lease.handle().index);
  EXPECT_TRUE(snapshot.isAlive());

  // Act: Release (RawLease - no ref counting, just invalidate)
  manager.release(lease);

  // Assert: library still alive (cache pattern)
  EXPECT_TRUE(snapshot.isAlive());
}

TYPED_TEST(MpsLibraryManagerTypedTest, ReleasedLeaseDoesNotAffectLibrary) {
  if constexpr (!TypeParam::is_mock) {
    GTEST_SKIP() << "Mock-only test";
    return;
  }
  auto &manager = this->manager();

  // Arrange
  this->initializeManager();
  const auto lib_handle = makeLibrary(0x890);
  this->adapter().expectCreateLibraries({{"Baz", lib_handle}});
  this->adapter().expectDestroyLibraries({lib_handle});

  // Act: Acquire same key twice (returns same cached library)
  auto first = manager.acquire(mps_rt::LibraryKey::Named("Baz"));
  auto second = manager.acquire(mps_rt::LibraryKey::Named("Baz"));

  // Assert: Both point to same library
  EXPECT_EQ(first.handle().index, second.handle().index);

  // Act: Release both (RawLease - no ref counting)
  manager.release(first);
  manager.release(second);

  // Assert: Library still alive (cache pattern)
  const auto &snapshot = manager.controlBlockForTest(0);
  EXPECT_TRUE(snapshot.isAlive());
}

TYPED_TEST(MpsLibraryManagerTypedTest, ReleaseIsIdempotent) {
  if constexpr (!TypeParam::is_mock) {
    GTEST_SKIP() << "Mock-only test";
    return;
  }
  auto &manager = this->manager();

  // Arrange
  this->initializeManager();
  const auto lib_handle = makeLibrary(0x8A0);
  this->adapter().expectCreateLibraries({{"Qux", lib_handle}});
  this->adapter().expectDestroyLibraries({lib_handle});

  auto lease = manager.acquire(mps_rt::LibraryKey::Named("Qux"));

  // Act & Assert: Multiple releases are safe
  manager.release(lease);
  manager.release(lease);
}

// =============================================================================
// PipelineManager Access Tests
// =============================================================================

TYPED_TEST(MpsLibraryManagerTypedTest,
           PipelineManagerProvidesNestedFunctionManager) {
  auto &manager = this->manager();
  this->initializeManager();

  if constexpr (!TypeParam::is_mock) {
    GTEST_SKIP() << "Mock-only test";
    return;
  }

  // Arrange
  const auto library_handle = makeLibrary(0x900);
  this->adapter().expectCreateLibraries({{"TestLib", library_handle}});
  this->adapter().expectDestroyLibraries({library_handle});

  // Act
  auto library_lease = manager.acquire(mps_rt::LibraryKey::Named("TestLib"));
  auto *pipeline_manager = manager.pipelineManager(library_lease);

  // Assert
  EXPECT_NE(pipeline_manager, nullptr);
  EXPECT_TRUE(pipeline_manager->isInitializedForTest());

  // Cleanup
  manager.release(library_lease);
}

TYPED_TEST(MpsLibraryManagerTypedTest, PipelineManagerCanBeAccessedByKey) {
  auto &manager = this->manager();
  this->initializeManager();

  if constexpr (!TypeParam::is_mock) {
    GTEST_SKIP() << "Mock-only test";
    return;
  }

  // Arrange
  const auto key = mps_rt::LibraryKey::Named("ByKey");
  const auto lib_handle = makeLibrary(0x910);
  this->adapter().expectCreateLibraries({{"ByKey", lib_handle}});
  this->adapter().expectDestroyLibraries({lib_handle});

  // Act: Access directly by key
  auto *pipeline_manager = manager.pipelineManager(key);

  // Assert: Library was created for the pipeline manager
  EXPECT_NE(pipeline_manager, nullptr);
  EXPECT_EQ(manager.capacity(), 1u);
}

TYPED_TEST(MpsLibraryManagerTypedTest, LibraryPersistsAfterLeaseRelease) {
  auto &manager = this->manager();
  this->initializeManager();

  if constexpr (!TypeParam::is_mock) {
    GTEST_SKIP() << "Mock-only test";
    return;
  }

  // Arrange
  const auto key = mps_rt::LibraryKey::Named("KeepAlive");
  const auto lib_handle = makeLibrary(0x930);
  this->adapter().expectCreateLibraries({{"KeepAlive", lib_handle}});
  this->adapter().expectDestroyLibraries({lib_handle});

  // Act
  auto library_lease = manager.acquire(key);
  auto *pipeline_manager = manager.pipelineManager(library_lease);
  EXPECT_NE(pipeline_manager, nullptr);

  manager.release(library_lease);

  // Assert: Library still alive after release
  const auto &snapshot_mid = manager.controlBlockForTest(0);
  EXPECT_TRUE(snapshot_mid.isAlive());
}

TYPED_TEST(MpsLibraryManagerTypedTest,
           PipelineManagerAccessByKeyReusesExistingLibrary) {
  auto &manager = this->manager();
  this->initializeManager();

  if constexpr (!TypeParam::is_mock) {
    GTEST_SKIP() << "Mock-only test";
    return;
  }

  // Arrange
  const auto key = mps_rt::LibraryKey::Named("ReuseByKey");
  const auto lib_handle = makeLibrary(0x940);
  this->adapter().expectCreateLibraries({{"ReuseByKey", lib_handle}});
  this->adapter().expectDestroyLibraries({lib_handle});

  // Act
  auto library_lease = manager.acquire(key);
  auto *pm1 = manager.pipelineManager(key);
  auto *pm2 = manager.pipelineManager(library_lease);

  // Assert: Same pipeline manager for same library
  EXPECT_EQ(pm1, pm2);

  manager.release(library_lease);

  // Assert: Library still alive after release
  const auto &snapshot = manager.controlBlockForTest(0);
  EXPECT_TRUE(snapshot.isAlive());
}

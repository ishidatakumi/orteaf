#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include <orteaf/internal/diagnostics/error/error.h>
#include <orteaf/internal/execution/mps/manager/mps_compute_pipeline_state_manager.h>
#include <orteaf/internal/execution/mps/manager/mps_library_manager.h>
#include <tests/internal/execution/mps/manager/testing/backend_ops_provider.h>
#include <tests/internal/execution/mps/manager/testing/manager_test_fixture.h>
#include <tests/internal/testing/error_assert.h>

namespace diag_error = orteaf::internal::diagnostics::error;
namespace mps_rt = orteaf::internal::execution::mps::manager;
namespace mps_wrapper = orteaf::internal::execution::mps::platform::wrapper;
namespace testing_mps = orteaf::tests::execution::mps::testing;

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
    mps_rt::MpsLibraryManager::Config config{};
    config.device = device;
    config.ops = this->getOps();
    config.payload_capacity = capacity;
    config.control_block_capacity = capacity;
    manager().configure(config);
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
  const auto device = this->adapter().device();

  // Assert: Default is 1
  EXPECT_EQ(manager.payloadGrowthChunkSizeForTest(), 1u);
  EXPECT_EQ(manager.controlBlockGrowthChunkSizeForTest(), 1u);

  // Act
  mps_rt::MpsLibraryManager::Config config{};
  config.device = device;
  config.ops = this->getOps();
  config.payload_growth_chunk_size = 3;
  config.control_block_growth_chunk_size = 4;
  manager.configure(config);

  // Assert
  EXPECT_EQ(manager.payloadGrowthChunkSizeForTest(), 3u);
  EXPECT_EQ(manager.controlBlockGrowthChunkSizeForTest(), 4u);
}

TYPED_TEST(MpsLibraryManagerTypedTest, GrowthChunkSizeRejectsZero) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Act & Assert: Zero is invalid
  ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
    mps_rt::MpsLibraryManager::Config config{};
    config.device = device;
    config.ops = this->getOps();
    config.payload_growth_chunk_size = 0;
    manager.configure(config);
  });
  ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
    mps_rt::MpsLibraryManager::Config config{};
    config.device = device;
    config.ops = this->getOps();
    config.control_block_growth_chunk_size = 0;
    manager.configure(config);
  });
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
  ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
    mps_rt::MpsLibraryManager::Config config{};
    config.device = nullptr;
    config.ops = this->getOps();
    config.payload_capacity = 1;
    config.control_block_capacity = 1;
    manager.configure(config);
  });
}

TYPED_TEST(MpsLibraryManagerTypedTest, InitializeRejectsNullOps) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
    mps_rt::MpsLibraryManager::Config config{};
    config.device = device;
    config.ops = nullptr;
    config.payload_capacity = 1;
    config.control_block_capacity = 1;
    manager.configure(config);
  });
}

TYPED_TEST(MpsLibraryManagerTypedTest, InitializeRejectsCapacityAboveLimit) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
    mps_rt::MpsLibraryManager::Config config{};
    config.device = device;
    config.ops = this->getOps();
    config.payload_capacity = std::numeric_limits<std::size_t>::max();
    config.control_block_capacity = std::numeric_limits<std::size_t>::max();
    manager.configure(config);
  });
}

TYPED_TEST(MpsLibraryManagerTypedTest, CapacityReflectsConfiguredPool) {
  auto &manager = this->manager();

  // Act
  this->initializeManager(2);

  // Assert: capacity is 2 after init with capacity=2
  EXPECT_EQ(manager.payloadPoolSizeForTest(), 2u);
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
  mps_rt::MpsLibraryManager::Config config{};
  config.device = this->adapter().device();
  config.ops = this->getOps();
  config.payload_growth_chunk_size = 3;
  manager.configure(config);
  const auto lib_handle = makeLibrary(0x800);
  this->adapter().expectCreateLibraries({{"GrowthTest0", lib_handle}});
  this->adapter().expectDestroyLibraries({lib_handle});

  // Act
  auto first = manager.acquire(mps_rt::LibraryKey::Named("GrowthTest0"));

  // Assert: capacity grows by growth chunk size (3)
  EXPECT_EQ(manager.payloadPoolSizeForTest(), 3u);

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
  auto *payload = lease.payloadPtr();
  ASSERT_NE(payload, nullptr);
  EXPECT_EQ(payload->library, lib_handle);

  // Save handle before release (release clears control block's payload handle)
  const auto saved_handle = lease.payloadHandle();
  EXPECT_TRUE(manager.payloadCreatedForTest(saved_handle));

  // Act: Release (RawLease - no ref counting, just invalidate)
  manager.release(lease);

  // Assert: library still alive (cache pattern) - use saved handle
  EXPECT_TRUE(manager.payloadCreatedForTest(saved_handle));
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
  EXPECT_EQ(first.payloadHandle().index, second.payloadHandle().index);

  // Save handle before release (release clears control block's payload handle)
  const auto saved_handle = first.payloadHandle();

  // Act: Release both (RawLease - no ref counting)
  manager.release(first);
  manager.release(second);

  // Assert: Library still alive (cache pattern) - use saved handle
  EXPECT_TRUE(manager.payloadCreatedForTest(saved_handle));
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
  auto *library_resource = library_lease.payloadPtr();
  ASSERT_NE(library_resource, nullptr);
  auto *pipeline_manager = &library_resource->pipeline_manager;

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
  auto library_lease = manager.acquire(key);
  auto *library_resource = library_lease.payloadPtr();
  ASSERT_NE(library_resource, nullptr);
  auto *pipeline_manager = &library_resource->pipeline_manager;

  // Assert: Library was created for the pipeline manager
  EXPECT_NE(pipeline_manager, nullptr);
  EXPECT_EQ(manager.payloadPoolSizeForTest(), 1u);
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
  auto *library_resource = library_lease.payloadPtr();
  ASSERT_NE(library_resource, nullptr);
  auto *pipeline_manager = &library_resource->pipeline_manager;
  EXPECT_NE(pipeline_manager, nullptr);

  // Save handle before release (release clears control block's payload handle)
  const auto saved_handle = library_lease.payloadHandle();

  manager.release(library_lease);

  // Assert: Library still alive after release - use saved handle
  EXPECT_TRUE(manager.payloadCreatedForTest(saved_handle));
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
  auto library_lease_again = manager.acquire(key);
  auto *resource_first = library_lease.payloadPtr();
  auto *resource_again = library_lease_again.payloadPtr();
  ASSERT_NE(resource_first, nullptr);
  ASSERT_NE(resource_again, nullptr);
  auto *pm1 = &resource_first->pipeline_manager;
  auto *pm2 = &resource_again->pipeline_manager;

  // Assert: Same pipeline manager for same library
  EXPECT_EQ(pm1, pm2);

  // Save handle before release (release clears control block's payload handle)
  const auto saved_handle = library_lease.payloadHandle();

  manager.release(library_lease);

  // Assert: Library still alive after release - use saved handle
  EXPECT_TRUE(manager.payloadCreatedForTest(saved_handle));
}

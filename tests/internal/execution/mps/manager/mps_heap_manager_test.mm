#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include <orteaf/internal/diagnostics/error/error.h>
#include <orteaf/internal/execution/mps/manager/mps_heap_manager.h>
#include <tests/internal/execution/mps/manager/testing/backend_ops_provider.h>
#include <tests/internal/execution/mps/manager/testing/manager_test_fixture.h>
#include <tests/internal/testing/error_assert.h>

namespace base = orteaf::internal::base;
namespace diag_error = orteaf::internal::diagnostics::error;
namespace mps_rt = orteaf::internal::execution::mps::manager;
namespace mps_wrapper = orteaf::internal::execution::mps::platform::wrapper;
namespace testing_mps = orteaf::tests::execution::mps::testing;

using orteaf::tests::ExpectError;

namespace {

mps_wrapper::MpsHeap_t makeHeap(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MpsHeap_t>(value);
}

mps_wrapper::MpsHeapDescriptor_t makeHeapDescriptor(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MpsHeapDescriptor_t>(value);
}

template <class Provider>
class MpsHeapManagerTypedTest
    : public testing_mps::RuntimeManagerFixture<Provider,
                                                mps_rt::MpsHeapManager> {
protected:
  using Base =
      testing_mps::RuntimeManagerFixture<Provider, mps_rt::MpsHeapManager>;

  mps_rt::MpsHeapManager &manager() { return Base::manager(); }
  auto &adapter() { return Base::adapter(); }

  mps_rt::HeapDescriptorKey defaultKey(std::size_t size = 0x1000) const {
    mps_rt::HeapDescriptorKey key{};
    key.size_bytes = size;
    return key;
  }

  mps_rt::MpsHeapManager::Config
  makeConfig(std::size_t payload_capacity = 0,
             std::size_t control_block_capacity = 0) {
    mps_rt::MpsHeapManager::Config config{};
    config.device = this->adapter().device();
    config.device_handle = base::DeviceHandle{0};
    config.library_manager = nullptr;
    config.ops = this->getOps();
    config.payload_capacity = payload_capacity;
    config.control_block_capacity = control_block_capacity;
    config.payload_growth_chunk_size = 1;
    config.control_block_growth_chunk_size = 1;
    return config;
  }

  void
  expectDescriptorConfiguration(const mps_rt::HeapDescriptorKey &key,
                                mps_wrapper::MpsHeapDescriptor_t descriptor,
                                bool expect_creation = true) {
    if constexpr (!Provider::is_mock) {
      (void)key;
      (void)descriptor;
      (void)expect_creation;
      return;
    }
    if (expect_creation) {
      this->adapter().expectCreateHeapDescriptors({descriptor});
    }
    this->adapter().expectSetHeapDescriptorSize({{descriptor, key.size_bytes}});
    this->adapter().expectSetHeapDescriptorResourceOptions(
        {{descriptor, key.resource_options}});
    this->adapter().expectSetHeapDescriptorStorageMode(
        {{descriptor, key.storage_mode}});
    this->adapter().expectSetHeapDescriptorCPUCacheMode(
        {{descriptor, key.cpu_cache_mode}});
    this->adapter().expectSetHeapDescriptorHazardTrackingMode(
        {{descriptor, key.hazard_tracking_mode}});
    this->adapter().expectSetHeapDescriptorType({{descriptor, key.heap_type}});
    this->adapter().expectDestroyHeapDescriptors({descriptor});
  }
};

#if ORTEAF_ENABLE_MPS
using ProviderTypes = ::testing::Types<testing_mps::MockBackendOpsProvider,
                                       testing_mps::RealBackendOpsProvider>;
#else
using ProviderTypes = ::testing::Types<testing_mps::MockBackendOpsProvider>;
#endif

} // namespace

TYPED_TEST_SUITE(MpsHeapManagerTypedTest, ProviderTypes);

// =============================================================================
// Configuration Tests
// =============================================================================

TYPED_TEST(MpsHeapManagerTypedTest, DefaultGrowthChunkSizeIsOne) {
  auto &manager = this->manager();
  auto config = this->makeConfig();

  // Act
  manager.configure(config);

  // Assert: Default control block growth chunk size is 1
  EXPECT_EQ(manager.controlBlockGrowthChunkSizeForTest(), 1u);

  // Cleanup
  manager.shutdown();
}

// =============================================================================
// Initialization Tests
// =============================================================================

TYPED_TEST(MpsHeapManagerTypedTest, AccessBeforeInitializationThrows) {
  auto &manager = this->manager();
  const auto key = this->defaultKey();

  // Act & Assert: Accessing before initialization throws InvalidState
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.acquire(key); });
}

TYPED_TEST(MpsHeapManagerTypedTest, ConfigureRejectsNullDevice) {
  auto &manager = this->manager();

  // Arrange
  auto config = this->makeConfig();
  config.device = nullptr;

  // Act & Assert: Null device is rejected
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { manager.configure(config); });
}

TYPED_TEST(MpsHeapManagerTypedTest, ConfigureRejectsNullOps) {
  auto &manager = this->manager();

  // Arrange
  auto config = this->makeConfig();
  config.ops = nullptr;

  // Act & Assert: Null ops is rejected
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { manager.configure(config); });
}

TYPED_TEST(MpsHeapManagerTypedTest, CapacityReflectsConfiguredPool) {
  auto &manager = this->manager();

  // Before initialization
  EXPECT_EQ(manager.payloadPoolCapacityForTest(), 0u);

  // After initialization: capacity is at least the configured size
  manager.configure(this->makeConfig(2, 2));
  EXPECT_GE(manager.payloadPoolCapacityForTest(), 2u);

  // After shutdown: capacity is preserved (FixedSlotStore behavior)
  manager.shutdown();
  // Note: FixedSlotStore preserves capacity after shutdown
}

// =============================================================================
// Heap Acquisition Tests
// =============================================================================

TYPED_TEST(MpsHeapManagerTypedTest, GrowthChunkControlsPoolExpansion) {
  auto &manager = this->manager();

  // Arrange
  auto config = this->makeConfig(0, 0);
  config.payload_growth_chunk_size = 3;
  manager.configure(config);

  const auto key = this->defaultKey();
  if constexpr (TypeParam::is_mock) {
    const auto descriptor = makeHeapDescriptor(0x1600);
    this->expectDescriptorConfiguration(key, descriptor);
    this->adapter().expectCreateHeapsInOrder({{descriptor, makeHeap(0x600)}});
  }

  // Act: Acquire one heap
  auto lease = manager.acquire(key);

  // Assert: Pool expanded by payloadGrowthChunkSize (3)
  EXPECT_GE(manager.payloadPoolCapacityForTest(), 3u);

  // Cleanup: Release lease before shutdown
  lease.release();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyHeaps({makeHeap(0x600)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsHeapManagerTypedTest, GetOrCreateCachesByDescriptor) {
  auto &manager = this->manager();
  manager.configure(this->makeConfig(1, 1));

  // Arrange
  const auto key = this->defaultKey();
  if constexpr (TypeParam::is_mock) {
    const auto descriptor = makeHeapDescriptor(0x1700);
    this->expectDescriptorConfiguration(key, descriptor);
    this->adapter().expectCreateHeapsInOrder({{descriptor, makeHeap(0x700)}});
  }

  // Act: Acquire same key twice
  auto first = manager.acquire(key);
  auto second = manager.acquire(key);

  // Assert: Same payload handle returned (cached)
  EXPECT_EQ(first.payloadHandle(), second.payloadHandle());

  // Assert
  EXPECT_TRUE(manager.isAliveForTest(first.payloadHandle()));

  // Cleanup: Release leases before shutdown
  first.release();
  second.release();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyHeaps({makeHeap(0x700)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsHeapManagerTypedTest, DistinctDescriptorsAllocateSeparateHeaps) {
  auto &manager = this->manager();
  manager.configure(this->makeConfig(0, 0));

  // Arrange: Create two different keys
  auto key_a = this->defaultKey(0x1800);
  auto key_b = this->defaultKey(0x2800);
  key_b.storage_mode = mps_wrapper::kMPSStorageModePrivate;
  key_b.heap_type = mps_wrapper::kMPSHeapTypePlacement;

  if constexpr (TypeParam::is_mock) {
    const auto descriptor_a = makeHeapDescriptor(0x1801);
    const auto descriptor_b = makeHeapDescriptor(0x2802);
    this->adapter().expectCreateHeapDescriptors({descriptor_a, descriptor_b});
    this->expectDescriptorConfiguration(key_a, descriptor_a, false);
    this->expectDescriptorConfiguration(key_b, descriptor_b, false);
    this->adapter().expectCreateHeapsInOrder(
        {{descriptor_a, makeHeap(0x801)}, {descriptor_b, makeHeap(0x802)}});
  }

  // Act: Acquire with different keys
  auto lease_a = manager.acquire(key_a);
  auto lease_b = manager.acquire(key_b);

  // Assert: Different handles
  EXPECT_NE(lease_a.payloadHandle(), lease_b.payloadHandle());

  // Cleanup: Release leases before shutdown
  lease_a.release();
  lease_b.release();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyHeaps({makeHeap(0x801), makeHeap(0x802)});
  }
  manager.shutdown();
}

// =============================================================================
// Release and Reuse Tests
// =============================================================================

TYPED_TEST(MpsHeapManagerTypedTest, ReleaseAllowsReuseWithoutRecreation) {
  auto &manager = this->manager();
  manager.configure(this->makeConfig(1, 1));

  // Arrange
  const auto key = this->defaultKey();
  if constexpr (TypeParam::is_mock) {
    const auto descriptor_first = makeHeapDescriptor(0x1900);
    this->expectDescriptorConfiguration(key, descriptor_first);
    this->adapter().expectCreateHeapsInOrder(
        {{descriptor_first, makeHeap(0x900)}});
  }

  // Act: Acquire, release, then reacquire
  auto lease = manager.acquire(key);
  const auto original_handle = lease.payloadHandle();
  lease.release();

  auto recreated = manager.acquire(key);

  // Assert: Same handle returned (cache pattern - heap persists)
  EXPECT_EQ(recreated.payloadHandle(), original_handle);

  // Cleanup: Release lease before shutdown
  recreated.release();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyHeaps({makeHeap(0x900)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsHeapManagerTypedTest, ManualReleaseInvalidatesLease) {
  auto &manager = this->manager();
  manager.configure(this->makeConfig(1, 1));

  // Arrange
  const auto key = this->defaultKey();
  if constexpr (TypeParam::is_mock) {
    const auto descriptor = makeHeapDescriptor(0x1D00);
    this->expectDescriptorConfiguration(key, descriptor);
    this->adapter().expectCreateHeapsInOrder({{descriptor, makeHeap(0xD00)}});
    this->adapter().expectDestroyHeaps({makeHeap(0xD00)});
  }

  // Act
  auto lease = manager.acquire(key);
  manager.release(lease);

  // Assert: Lease is invalidated after release
  EXPECT_FALSE(static_cast<bool>(lease));

  // Cleanup
  manager.shutdown();
}

// =============================================================================
// Validation Tests
// =============================================================================

TYPED_TEST(MpsHeapManagerTypedTest, DescriptorSizeMustBePositive) {
  auto &manager = this->manager();
  manager.configure(this->makeConfig(0, 0));

  // Arrange: Create invalid key
  mps_rt::HeapDescriptorKey key{};
  key.size_bytes = 0;

  // Act & Assert: Zero size is rejected
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { (void)manager.acquire(key); });

  manager.shutdown();
}

// =============================================================================
// Shutdown Tests
// =============================================================================

TYPED_TEST(MpsHeapManagerTypedTest, ShutdownDestroysRemainingHeaps) {
  auto &manager = this->manager();
  manager.configure(this->makeConfig(1, 1));

  // Arrange
  const auto key = this->defaultKey(0x1F00);
  if constexpr (TypeParam::is_mock) {
    const auto descriptor = makeHeapDescriptor(0x1B00);
    this->expectDescriptorConfiguration(key, descriptor);
    this->adapter().expectCreateHeapsInOrder({{descriptor, makeHeap(0xB00)}});
    this->adapter().expectDestroyHeaps({makeHeap(0xB00)});
  }

  // Act: Acquire but don't release
  (void)manager.acquire(key);

  // Assert: Shutdown cleans up remaining heaps (verified by mock expectations)
  manager.shutdown();
}

// =============================================================================
// BufferManager Access Tests
// =============================================================================

TYPED_TEST(MpsHeapManagerTypedTest, BufferManagerAccessFromLease) {
  if constexpr (!TypeParam::is_mock) {
    GTEST_SKIP() << "Mock-only test";
    return;
  }
  auto &manager = this->manager();

  // Arrange
  manager.configure(this->makeConfig(1, 1));

  const auto key = this->defaultKey(0x2000);
  const auto descriptor = makeHeapDescriptor(0x2100);
  this->expectDescriptorConfiguration(key, descriptor);
  this->adapter().expectCreateHeapsInOrder({{descriptor, makeHeap(0xD00)}});
  this->adapter().expectDestroyHeaps({makeHeap(0xD00)});

  // Act: Acquire heap, then get buffer manager
  auto lease = manager.acquire(key);
  auto *buffer_manager = manager.bufferManager(lease);

  // Assert: BufferManager is valid and initialized
  EXPECT_NE(buffer_manager, nullptr);
  EXPECT_TRUE(buffer_manager->isInitializedForTest());

  // Cleanup: Release lease before shutdown
  lease.release();
  manager.shutdown();
}

TYPED_TEST(MpsHeapManagerTypedTest, BufferManagerAccessFromKey) {
  if constexpr (!TypeParam::is_mock) {
    GTEST_SKIP() << "Mock-only test";
    return;
  }
  auto &manager = this->manager();

  // Arrange
  manager.configure(this->makeConfig(1, 1));

  const auto key = this->defaultKey(0x3000);
  const auto descriptor = makeHeapDescriptor(0x3100);
  this->expectDescriptorConfiguration(key, descriptor);
  this->adapter().expectCreateHeapsInOrder({{descriptor, makeHeap(0xE00)}});
  this->adapter().expectDestroyHeaps({makeHeap(0xE00)});

  // Act: Get buffer manager directly by key (creates heap if needed)
  auto *buffer_manager = manager.bufferManager(key);

  // Assert: BufferManager is valid
  EXPECT_NE(buffer_manager, nullptr);
  EXPECT_TRUE(buffer_manager->isInitializedForTest());

  // Act: Same key returns same buffer manager (cached)
  auto *buffer_manager2 = manager.bufferManager(key);

  // Assert: Same pointer
  EXPECT_EQ(buffer_manager, buffer_manager2);

  // Cleanup
  manager.shutdown();
}

TYPED_TEST(MpsHeapManagerTypedTest,
           BufferManagerAccessBeforeInitializationThrows) {
  auto &manager = this->manager();
  const auto key = this->defaultKey(0x4000);

  // Act & Assert: Accessing before initialization throws InvalidState
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.bufferManager(key); });
}

TYPED_TEST(MpsHeapManagerTypedTest, BufferManagerAccessWithInvalidKeyThrows) {
  auto &manager = this->manager();

  // Arrange
  manager.configure(this->makeConfig(1, 1));

  // Create invalid key
  mps_rt::HeapDescriptorKey key{};
  key.size_bytes = 0;

  // Act & Assert: Invalid key is rejected
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { (void)manager.bufferManager(key); });

  manager.shutdown();
}

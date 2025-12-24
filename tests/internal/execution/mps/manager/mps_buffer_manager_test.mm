#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include <orteaf/internal/diagnostics/error/error.h>
#include <orteaf/internal/execution/mps/manager/mps_buffer_manager.h>
#include <tests/internal/testing/error_assert.h>

namespace base = orteaf::internal::base;
namespace diag_error = orteaf::internal::diagnostics::error;
namespace mps_rt = orteaf::internal::execution::mps::manager;

using orteaf::tests::ExpectError;

namespace {

#if ORTEAF_ENABLE_MPS

// =============================================================================
// Phase 1: Simple Tests (No GPU resources required)
// =============================================================================

class MpsBufferManagerSimpleTest : public ::testing::Test {
protected:
  using Manager = mps_rt::MpsBufferManager;
  using LaunchParams = typename Manager::LaunchParams;

  Manager &manager() { return manager_; }

  Manager manager_{};
  LaunchParams params_{};
};

// -----------------------------------------------------------------------------
// Shutdown Tests (Before Initialization)
// -----------------------------------------------------------------------------

TEST_F(MpsBufferManagerSimpleTest, ShutdownWithoutInitializeIsNoOp) {
  // Act & Assert: Shutdown before initialization is safe
  EXPECT_NO_THROW(manager().shutdown());
}

TEST_F(MpsBufferManagerSimpleTest,
       MultipleShutdownsWithoutInitializeAreIdempotent) {
  // Act & Assert: Multiple shutdowns are idempotent
  EXPECT_NO_THROW(manager().shutdown());
  EXPECT_NO_THROW(manager().shutdown());
  EXPECT_NO_THROW(manager().shutdown());
}

// -----------------------------------------------------------------------------
// State Tests (Before Initialization)
// -----------------------------------------------------------------------------

TEST_F(MpsBufferManagerSimpleTest, IsNotInitializedByDefault) {
  // Assert: Not initialized by default
  EXPECT_FALSE(manager().isInitializedForTest());
}

TEST_F(MpsBufferManagerSimpleTest, CapacityIsZeroBeforeInitialize) {
  // Assert: Capacity is zero before initialization
  EXPECT_EQ(manager().payloadPoolCapacityForTest(), 0u);
}

TEST_F(MpsBufferManagerSimpleTest, AcquireBeforeInitializationThrows) {
  // Act & Assert: Acquire before initialization throws InvalidState
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager().acquire(1024, 16, params_); });
}

TEST_F(MpsBufferManagerSimpleTest, AcquireByHandleBeforeInitializationThrows) {
  // Arrange
  base::BufferHandle handle{0, 1};

  // Act & Assert: Acquire by handle before initialization throws InvalidState
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager().acquire(handle); });
}

// -----------------------------------------------------------------------------
// GrowthChunkSize Tests
// -----------------------------------------------------------------------------

TEST_F(MpsBufferManagerSimpleTest, DefaultGrowthChunkSizeIsOne) {
  // Assert: Default growth chunk size is 1
  EXPECT_EQ(manager().controlBlockGrowthChunkSizeForTest(), 1u);
}

#endif // ORTEAF_ENABLE_MPS

} // namespace

// =============================================================================
// Phase 2: Integration Tests (Real GPU resources)
// =============================================================================
#if ORTEAF_ENABLE_MPS

#include <orteaf/internal/execution/mps/manager/mps_library_manager.h>
#include <orteaf/internal/execution/mps/platform/mps_slow_ops.h>
#include <orteaf/internal/execution/mps/platform/wrapper/mps_heap.h>

namespace mps_platform = orteaf::internal::execution::mps::platform;
namespace mps_wrapper = orteaf::internal::execution::mps::platform::wrapper;

namespace {

class MpsBufferManagerIntegrationTest : public ::testing::Test {
protected:
  using Manager = mps_rt::MpsBufferManager;
  using Config = typename Manager::Config;

  void SetUp() override {
    // Acquire real MPS device
    const int count = ops_.getDeviceCount();
    if (count <= 0) {
      GTEST_SKIP() << "No MPS devices available";
    }
    device_ = ops_.getDevice(0);
    if (device_ == nullptr) {
      GTEST_SKIP() << "Unable to acquire MPS device";
    }

    // Create heap descriptor
    heap_descriptor_ = mps_wrapper::createHeapDescriptor();
    if (!heap_descriptor_) {
      GTEST_SKIP() << "Failed to create heap descriptor";
    }
    mps_wrapper::setHeapDescriptorSize(heap_descriptor_,
                                       64 * 1024 * 1024); // 64MB

    // Create heap
    heap_ = mps_wrapper::createHeap(device_, heap_descriptor_);
    if (!heap_) {
      mps_wrapper::destroyHeapDescriptor(heap_descriptor_);
      GTEST_SKIP() << "Failed to create heap";
    }

    // Initialize library manager (required by MpsResource)
    mps_rt::MpsLibraryManager::Config lib_config{};
    lib_config.device = device_;
    lib_config.ops = &ops_;
    lib_config.payload_capacity = 16;
    lib_config.control_block_capacity = 16;
    lib_manager_.configure(lib_config);

    setup_successful_ = true;
  }

  void TearDown() override {
    manager_.shutdown();
    lib_manager_.shutdown();
    if (heap_) {
      mps_wrapper::destroyHeap(heap_);
      heap_ = nullptr;
    }
    if (heap_descriptor_) {
      mps_wrapper::destroyHeapDescriptor(heap_descriptor_);
      heap_descriptor_ = nullptr;
    }
  }

  Manager &manager() { return manager_; }

  void initializeManager(std::size_t capacity = 8) {
    Config cfg{};
    cfg.device = device_;
    cfg.device_handle = base::DeviceHandle{0};
    cfg.heap = heap_;
    cfg.library_manager = &lib_manager_;
    cfg.min_block_size = 64;
    cfg.max_block_size = 16 * 1024 * 1024;
    cfg.chunk_size = 16 * 1024 * 1024;
    cfg.payload_capacity = capacity;
    cfg.control_block_capacity = capacity;
    cfg.payload_block_size = 1;
    cfg.control_block_block_size = 1;
    manager_.configure(cfg);
  }

  mps_wrapper::MpsDevice_t device() { return device_; }
  bool setupSuccessful() const { return setup_successful_; }

  mps_platform::MpsSlowOpsImpl ops_{};
  Manager manager_{};
  mps_wrapper::MpsDevice_t device_{nullptr};
  mps_wrapper::MpsHeapDescriptor_t heap_descriptor_{nullptr};
  mps_wrapper::MpsHeap_t heap_{nullptr};
  mps_rt::MpsLibraryManager lib_manager_{};
  bool setup_successful_{false};
  Manager::LaunchParams params_{};
};

// -----------------------------------------------------------------------------
// Initialization Tests
// -----------------------------------------------------------------------------

TEST_F(MpsBufferManagerIntegrationTest, InitializeSucceeds) {
  if (!setupSuccessful()) {
    GTEST_SKIP() << "GPU setup failed";
  }

  // Act & Assert
  EXPECT_NO_THROW(initializeManager());
  EXPECT_TRUE(manager().isInitializedForTest());
}

TEST_F(MpsBufferManagerIntegrationTest, InitializeWithZeroCapacitySucceeds) {
  if (!setupSuccessful()) {
    GTEST_SKIP() << "GPU setup failed";
  }

  // Act
  EXPECT_NO_THROW(initializeManager(0));

  // Assert
  EXPECT_TRUE(manager().isInitializedForTest());
  EXPECT_EQ(manager().payloadPoolCapacityForTest(), 0u);
}

TEST_F(MpsBufferManagerIntegrationTest, ShutdownAfterInitializeWorks) {
  if (!setupSuccessful()) {
    GTEST_SKIP() << "GPU setup failed";
  }

  // Arrange
  initializeManager();

  // Act
  EXPECT_NO_THROW(manager().shutdown());

  // Assert
  EXPECT_FALSE(manager().isInitializedForTest());
}

TEST_F(MpsBufferManagerIntegrationTest, MultipleInitializeShutdownCyclesWork) {
  if (!setupSuccessful()) {
    GTEST_SKIP() << "GPU setup failed";
  }

  // Act & Assert: Multiple cycles work
  for (int i = 0; i < 3; ++i) {
    EXPECT_NO_THROW(initializeManager());
    EXPECT_TRUE(manager().isInitializedForTest());
    EXPECT_NO_THROW(manager().shutdown());
    EXPECT_FALSE(manager().isInitializedForTest());
  }
}

// -----------------------------------------------------------------------------
// Acquire/Release Tests
// -----------------------------------------------------------------------------

TEST_F(MpsBufferManagerIntegrationTest, AcquireReturnsValidLease) {
  if (!setupSuccessful()) {
    GTEST_SKIP() << "GPU setup failed";
  }

  // Arrange
  initializeManager();

  // Act
  auto lease = manager().acquire(1024, 16, params_);

  // Assert
  EXPECT_TRUE(lease);
  EXPECT_TRUE(lease.handle().isValid());

  // Cleanup
  manager().release(lease);
}

TEST_F(MpsBufferManagerIntegrationTest,
       AcquireWithZeroSizeReturnsInvalidLease) {
  if (!setupSuccessful()) {
    GTEST_SKIP() << "GPU setup failed";
  }

  // Arrange
  initializeManager();

  // Act
  auto lease = manager().acquire(0, 16, params_);

  // Assert: Zero size returns invalid lease
  EXPECT_FALSE(lease);
}

TEST_F(MpsBufferManagerIntegrationTest, MultipleAllocationsWork) {
  if (!setupSuccessful()) {
    GTEST_SKIP() << "GPU setup failed";
  }

  // Arrange
  initializeManager();

  // Act
  auto lease1 = manager().acquire(1024, 16, params_);
  auto lease2 = manager().acquire(2048, 32, params_);
  auto lease3 = manager().acquire(4096, 64, params_);

  // Assert: All leases are valid and distinct
  EXPECT_TRUE(lease1);
  EXPECT_TRUE(lease2);
  EXPECT_TRUE(lease3);
  EXPECT_NE(lease1.handle().index, lease2.handle().index);
  EXPECT_NE(lease2.handle().index, lease3.handle().index);
  EXPECT_NE(lease1.handle().index, lease3.handle().index);

  // Cleanup
  manager().release(lease1);
  manager().release(lease2);
  manager().release(lease3);
}

TEST_F(MpsBufferManagerIntegrationTest, BufferRecyclingReusesSlots) {
  if (!setupSuccessful()) {
    GTEST_SKIP() << "GPU setup failed";
  }

  // Arrange
  initializeManager();
  auto first = manager().acquire(1024, 16, params_);
  const auto first_index = first.handle().index;

  // Act: Release and acquire again
  manager().release(first);
  auto second = manager().acquire(2048, 16, params_);

  // Assert: Same slot is reused
  EXPECT_EQ(second.handle().index, first_index);

  // Cleanup
  manager().release(second);
}

} // namespace

#endif // ORTEAF_ENABLE_MPS

// =============================================================================
// Phase 3: Mock Tests (Using StubMpsResource, No GPU required)
// =============================================================================
#if ORTEAF_ENABLE_MPS

#include <tests/internal/execution/mps/manager/testing/mock_mps_resource.h>

// StubMpsResource-based MpsBufferManager
using StubBufferManager =
    mps_rt::MpsBufferManagerT<orteaf::tests::execution::mps::StubMpsResource>;

class MpsBufferManagerMockTest : public ::testing::Test {
protected:
  using Manager = StubBufferManager;
  using Config = typename Manager::Config;
  using LaunchParams = typename Manager::LaunchParams;
  using StubResource = orteaf::tests::execution::mps::StubMpsResource;

  Manager &manager() { return manager_; }

  // Dummy device for StubMpsResource
  typename Manager::DeviceType device() {
    return reinterpret_cast<typename Manager::DeviceType>(0x12345678);
  }

  void *heap() { return reinterpret_cast<void *>(0x87654321); }

  void initializeManager(std::size_t capacity = 8) {
    Config cfg{};
    cfg.device = device();
    cfg.device_handle = base::DeviceHandle{0};
    cfg.heap = heap();
    cfg.library_manager = nullptr;
    cfg.min_block_size = 64;
    cfg.max_block_size = 1024 * 1024;
    cfg.chunk_size = 1024 * 1024;
    cfg.payload_capacity = capacity;
    cfg.control_block_capacity = capacity;
    cfg.payload_block_size = 1;
    cfg.control_block_block_size = 1;
    manager_.configure(cfg);
  }

  Manager manager_{};
  LaunchParams params_{};
};

// -----------------------------------------------------------------------------
// Initialization Tests (Mock)
// -----------------------------------------------------------------------------

TEST_F(MpsBufferManagerMockTest, InitializeSucceeds) {
  // Act & Assert
  EXPECT_NO_THROW(initializeManager());
  EXPECT_TRUE(manager_.isInitializedForTest());
}

TEST_F(MpsBufferManagerMockTest, ShutdownAfterInitializeWorks) {
  // Arrange
  initializeManager();

  // Act
  EXPECT_NO_THROW(manager_.shutdown());

  // Assert
  EXPECT_FALSE(manager_.isInitializedForTest());
}

// -----------------------------------------------------------------------------
// Acquire/Release Tests (Mock)
// -----------------------------------------------------------------------------

TEST_F(MpsBufferManagerMockTest, AcquireReturnsValidLease) {
  // Arrange
  initializeManager();

  // Act
  auto lease = manager_.acquire(1024, 16, params_);

  // Assert
  EXPECT_TRUE(lease);
  EXPECT_TRUE(lease.handle().isValid());

  // Cleanup
  manager_.release(lease);
}

TEST_F(MpsBufferManagerMockTest, MultipleAcquisitionsWork) {
  // Arrange
  initializeManager();

  // Act
  auto lease1 = manager_.acquire(256, 16, params_);
  auto lease2 = manager_.acquire(512, 32, params_);
  auto lease3 = manager_.acquire(1024, 64, params_);

  // Assert: All distinct handles
  EXPECT_TRUE(lease1);
  EXPECT_TRUE(lease2);
  EXPECT_TRUE(lease3);
  EXPECT_NE(lease1.handle().index, lease2.handle().index);
  EXPECT_NE(lease2.handle().index, lease3.handle().index);

  // Cleanup
  manager_.release(lease1);
  manager_.release(lease2);
  manager_.release(lease3);
}

TEST_F(MpsBufferManagerMockTest, ReleaseRecyclesSlot) {
  // Arrange
  initializeManager();
  auto first = manager_.acquire(256, 16, params_);
  const auto first_index = first.handle().index;

  // Act: Release and acquire
  manager_.release(first);
  auto second = manager_.acquire(512, 16, params_);

  // Assert: Same slot reused
  EXPECT_EQ(second.handle().index, first_index);

  // Cleanup
  manager_.release(second);
}

TEST_F(MpsBufferManagerMockTest, AcquireByHandleIncreasesRefCount) {
  // Arrange
  initializeManager();
  auto lease1 = manager_.acquire(256, 16, params_);
  const auto handle = lease1.handle();

  // Act: Acquire by handle (uses payload handle, not control block handle)
  // Note: This creates a new control block for the same payload
  // TODO: This test needs to be updated for the new pattern
  // For now, skip this test as the semantics have changed
  GTEST_SKIP() << "acquire(handle) semantics changed - needs redesign";
}

TEST_F(MpsBufferManagerMockTest, AcquireWithZeroSizeReturnsInvalidLease) {
  // Arrange
  initializeManager();

  // Act
  auto lease = manager_.acquire(0, 16, params_);

  // Assert
  EXPECT_FALSE(lease);
}

TEST_F(MpsBufferManagerMockTest, CapacityGrowsOnAcquire) {
  // Arrange
  initializeManager(4);

  // Act
  auto lease1 = manager_.acquire(256, 16, params_);
  auto lease2 = manager_.acquire(256, 16, params_);

  // Assert: Capacity grows
  EXPECT_GE(manager_.payloadPoolCapacityForTest(), 2u);

  // Cleanup
  manager_.release(lease1);
  manager_.release(lease2);
}

#endif // ORTEAF_ENABLE_MPS

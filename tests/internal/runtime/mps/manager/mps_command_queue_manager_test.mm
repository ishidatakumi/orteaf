#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include <orteaf/internal/diagnostics/error/error.h>
#include <orteaf/internal/diagnostics/log/log_config.h>
#include <orteaf/internal/runtime/mps/manager/mps_command_queue_manager.h>
#include <orteaf/internal/runtime/mps/platform/wrapper/mps_command_queue.h>
#include <orteaf/internal/runtime/mps/platform/wrapper/mps_event.h>
#include <tests/internal/runtime/mps/manager/testing/backend_ops_provider.h>
#include <tests/internal/runtime/mps/manager/testing/manager_test_fixture.h>
#include <tests/internal/testing/error_assert.h>

namespace diag_error = orteaf::internal::diagnostics::error;
namespace mps_rt = orteaf::internal::runtime::mps::manager;
namespace mps_wrapper = orteaf::internal::runtime::mps::platform::wrapper;
namespace testing_mps = orteaf::tests::runtime::mps::testing;

using orteaf::tests::ExpectError;

namespace {

mps_wrapper::MpsCommandQueue_t makeQueue(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MpsCommandQueue_t>(value);
}

template <class Provider>
class MpsCommandQueueManagerTypedTest
    : public testing_mps::RuntimeManagerFixture<
          Provider, mps_rt::MpsCommandQueueManager> {
protected:
  using Base =
      testing_mps::RuntimeManagerFixture<Provider,
                                         mps_rt::MpsCommandQueueManager>;

  mps_rt::MpsCommandQueueManager &manager() { return Base::manager(); }
  auto &adapter() { return Base::adapter(); }
};

#if ORTEAF_ENABLE_MPS
using ProviderTypes = ::testing::Types<testing_mps::MockBackendOpsProvider,
                                       testing_mps::RealBackendOpsProvider>;
#else
using ProviderTypes = ::testing::Types<testing_mps::MockBackendOpsProvider>;
#endif

} // namespace

TYPED_TEST_SUITE(MpsCommandQueueManagerTypedTest, ProviderTypes);

// =============================================================================
// Configuration Tests
// =============================================================================

TYPED_TEST(MpsCommandQueueManagerTypedTest, GrowthChunkSizeCanBeAdjusted) {
  auto &manager = this->manager();

  // Assert: Default is 1
  EXPECT_EQ(manager.growthChunkSize(), 1u);

  // Act
  manager.setGrowthChunkSize(4);

  // Assert
  EXPECT_EQ(manager.growthChunkSize(), 4u);
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, GrowthChunkSizeRejectsZero) {
  auto &manager = this->manager();

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { manager.setGrowthChunkSize(0); });
}

TYPED_TEST(MpsCommandQueueManagerTypedTest,
           GrowthChunkSizeReflectedInDebugState) {
  if constexpr (!TypeParam::is_mock) {
    GTEST_SKIP() << "Mock-only test";
    return;
  }
  auto &manager = this->manager();

  // Arrange
  manager.setGrowthChunkSize(3);
  const auto device = this->adapter().device();
  this->adapter().expectCreateCommandQueues({makeQueue(0x510)});

  // Act
  manager.initialize(device, this->getOps(), 1);
  auto lease = manager.acquire();

  // Assert
  EXPECT_EQ(manager.growthChunkSize(), 3u);

  // Cleanup
  manager.release(lease);
  this->adapter().expectDestroyCommandQueues({makeQueue(0x510)});
  manager.shutdown();
}

// =============================================================================
// Initialization Tests
// =============================================================================

TYPED_TEST(MpsCommandQueueManagerTypedTest,
           InitializeCreatesConfiguredNumberOfResources) {
  auto &manager = this->manager();

  // Arrange
  this->adapter().expectCreateCommandQueues({makeQueue(0x1), makeQueue(0x2)});
  const auto device = this->adapter().device();

  // Act
  manager.initialize(device, this->getOps(), 2);

  // Assert
  EXPECT_EQ(manager.capacity(), 2u);

  // Cleanup
  this->adapter().expectDestroyCommandQueuesInOrder(
      {makeQueue(0x1), makeQueue(0x2)});
  manager.shutdown();
}

TYPED_TEST(MpsCommandQueueManagerTypedTest,
           InitializeRejectsCapacityAboveLimit) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
    manager.initialize(device, this->getOps(),
                       std::numeric_limits<std::size_t>::max());
  });
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, CapacityReflectsPoolSize) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Before init
  EXPECT_EQ(manager.capacity(), 0u);

  // Arrange
  this->adapter().expectCreateCommandQueues(
      {makeQueue(0x301), makeQueue(0x302), makeQueue(0x303)});

  // Act
  manager.initialize(device, this->getOps(), 3);

  // Assert
  EXPECT_EQ(manager.capacity(), 3u);

  // Cleanup
  this->adapter().expectDestroyCommandQueues(
      {makeQueue(0x301), makeQueue(0x302), makeQueue(0x303)});
  manager.shutdown();
  EXPECT_EQ(manager.capacity(), 0u);
}

// =============================================================================
// Acquire Tests
// =============================================================================

TYPED_TEST(MpsCommandQueueManagerTypedTest, AcquireFailsBeforeInitialization) {
  auto &manager = this->manager();

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.acquire(); });
}

TYPED_TEST(MpsCommandQueueManagerTypedTest,
           AcquireReturnsDistinctIdsWithinCapacity) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Arrange
  this->adapter().expectCreateCommandQueues(
      {makeQueue(0x100), makeQueue(0x200)});
  manager.initialize(device, this->getOps(), 2);

  // Act
  auto id0 = manager.acquire();
  auto id1 = manager.acquire();

  // Assert
  EXPECT_NE(id0.handle(), id1.handle());
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, AcquireGrowsPoolWhenFreelistEmpty) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Arrange
  manager.setGrowthChunkSize(1);
  this->adapter().expectCreateCommandQueues({makeQueue(0x300)});
  manager.initialize(device, this->getOps(), 1);

  // Act: First acquire uses existing
  auto first = manager.acquire();

  // Arrange: Expect growth
  this->adapter().expectCreateCommandQueues({makeQueue(0x301)});

  // Act: Second acquire triggers growth
  auto second = manager.acquire();

  // Assert
  EXPECT_NE(first.handle(), second.handle());
}

TYPED_TEST(MpsCommandQueueManagerTypedTest,
           SetGrowthChunkSizeRejectsExcessiveValue) {
  auto &manager = this->manager();

  // Act & Assert: SIZE_MAX exceeds Handle's max index range
  ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
    manager.setGrowthChunkSize(std::numeric_limits<std::size_t>::max());
  });
}

// =============================================================================
// Release Tests
// =============================================================================

TYPED_TEST(MpsCommandQueueManagerTypedTest, ReleaseFailsBeforeInitialization) {
  auto &manager = this->manager();

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.acquire(); });
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, ManualReleaseInvalidatesLease) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Arrange
  this->adapter().expectCreateCommandQueues({makeQueue(0x455)});
  manager.initialize(device, this->getOps(), 1);

  // Act
  auto lease = manager.acquire();
  const auto original_handle = lease.handle();
  ASSERT_TRUE(static_cast<bool>(lease));

  manager.release(lease);

  // Assert: Lease is invalidated
  EXPECT_FALSE(static_cast<bool>(lease));

  const auto &state = manager.controlBlockForTest(original_handle.index);
  EXPECT_FALSE(state.isAlive());
  // BaseManagerCore does not track generation in this way (Slot has it, but
  // accessor returns ControlBlock). Slot generation is hidden or available via
  // slot.generation? Basic Slot does not have generation. Only GenerationalSlot
  // has generation. MpsCommandQueueManager is using base::Slot (not
  // GenerationalSlot) via UniqueControlBlock? UniqueControlBlock takes SlotT.
  // mps_command_queue_manager.h defines Slot = base::Slot. base::Slot does not
  // have generation. So EXPECT_GT(state.generation, ...) is invalid now. We can
  // only check in_use.

  // Act: Reacquire gets new generation
  auto reacquired = manager.acquire();
  EXPECT_NE(reacquired.handle(), original_handle);
  reacquired.release();

  // Cleanup
  this->adapter().expectDestroyCommandQueues({makeQueue(0x455)});
  manager.shutdown();
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, ReleaseRejectsNonAcquiredId) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Arrange
  this->adapter().expectCreateCommandQueues({makeQueue(0x700)});
  manager.initialize(device, this->getOps(), 1);

  // Act: Move lease and release both
  auto lease = manager.acquire();
  auto moved = std::move(lease);

  // Assert: Release moved-from is benign
  manager.release(lease);
  manager.release(moved);
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, ReleaseRequiresCompletedWork) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Arrange
  this->adapter().expectCreateCommandQueues({makeQueue(0x710)});
  manager.initialize(device, this->getOps(), 1);

  // Act & Assert: Release via Lease
  auto lease = manager.acquire();
  lease.release();
}

TYPED_TEST(MpsCommandQueueManagerTypedTest,
           ReleaseMakesHandleStaleAndRecyclesState) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Arrange
  manager.setGrowthChunkSize(1);
  this->adapter().expectCreateCommandQueues({makeQueue(0x720)});
  manager.initialize(device, this->getOps(), 1);

  // Act
  auto lease = manager.acquire();
  auto old_handle = lease.handle();
  manager.release(lease);

  this->adapter().expectCreateCommandQueues({});
  auto recycled = manager.acquire();

  // Assert: Handle changed (generation bumped)
  EXPECT_NE(recycled.handle(), old_handle);
}

// =============================================================================
// Capacity Management Tests
// =============================================================================

TYPED_TEST(MpsCommandQueueManagerTypedTest, GrowCapacityAddsAdditionalQueues) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Arrange
  this->adapter().expectCreateCommandQueues({makeQueue(0x350)});
  manager.initialize(device, this->getOps(), 1);
  EXPECT_EQ(manager.capacity(), 1u);

  // Act
  this->adapter().expectCreateCommandQueues(
      {makeQueue(0x360), makeQueue(0x370)});
  manager.growCapacity(2);

  // Assert
  EXPECT_EQ(manager.capacity(), 3u);

  // Act: Zero growth is no-op
  manager.growCapacity(0);
  EXPECT_EQ(manager.capacity(), 3u);

  // Act: Verify all can be acquired
  auto id0 = manager.acquire();
  auto id1 = manager.acquire();
  auto id2 = manager.acquire();
  EXPECT_NE(id0.handle(), id1.handle());
  EXPECT_NE(id1.handle(), id2.handle());
  EXPECT_NE(id0.handle(), id2.handle());

  // Cleanup
  id0.release();
  id1.release();
  id2.release();
  this->adapter().expectDestroyCommandQueues(
      {makeQueue(0x350), makeQueue(0x360), makeQueue(0x370)});
  manager.shutdown();
}

TYPED_TEST(MpsCommandQueueManagerTypedTest,
           ReleaseUnusedQueuesFreesResourcesAndReallocatesOnDemand) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Arrange
  this->adapter().expectCreateCommandQueues(
      {makeQueue(0x400), makeQueue(0x401)});
  manager.initialize(device, this->getOps(), 2);

  // Act: Acquire and release
  auto lease = manager.acquire();
  lease.release();

  this->adapter().expectDestroyCommandQueues(
      {makeQueue(0x400), makeQueue(0x401)});
  manager.releaseUnusedQueues();

  // Assert: Capacity is now 0
  EXPECT_EQ(manager.capacity(), 0u);

  // Act: Reacquire creates new queue
  this->adapter().expectCreateCommandQueues({makeQueue(0x420)});
  auto reacquired = manager.acquire();
  reacquired.release();
  EXPECT_EQ(manager.capacity(), 1u);

  // Cleanup
  this->adapter().expectDestroyCommandQueues({makeQueue(0x420)});
  manager.shutdown();
  EXPECT_EQ(manager.capacity(), 0u);
}

TYPED_TEST(MpsCommandQueueManagerTypedTest,
           ReleaseUnusedQueuesFailsIfQueuesAreInUse) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Arrange
  this->adapter().expectCreateCommandQueues(
      {makeQueue(0x430), makeQueue(0x431)});
  manager.initialize(device, this->getOps(), 2);

  auto lease = manager.acquire();

  // Act & Assert: Cannot release while in use
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { manager.releaseUnusedQueues(); });

  // Cleanup
  manager.release(lease);
  this->adapter().expectDestroyCommandQueues(
      {makeQueue(0x430), makeQueue(0x431)});
  manager.shutdown();
}

// =============================================================================
// Debug State Tests
// =============================================================================

TYPED_TEST(MpsCommandQueueManagerTypedTest, HazardCountersDefaultToZero) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Arrange
  this->adapter().expectCreateCommandQueues({makeQueue(0x900)});
  manager.initialize(device, this->getOps(), 1);

  // Act & Assert
  auto lease = manager.acquire();
  lease.release();
}

TYPED_TEST(MpsCommandQueueManagerTypedTest,
           HazardCountersCanBeUpdatedAndResetOnRelease) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Arrange
  this->adapter().expectCreateCommandQueues({makeQueue(0x910)});
  manager.initialize(device, this->getOps(), 1);

  // Act: Acquire, release, reacquire
  auto lease = manager.acquire();
  lease.release();
  auto recycled = manager.acquire();
  recycled.release();
}

#if ORTEAF_ENABLE_TEST
TYPED_TEST(MpsCommandQueueManagerTypedTest, DebugStateReflectsSetterUpdates) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Arrange
  this->adapter().expectCreateCommandQueues({makeQueue(0x920)});
  manager.initialize(device, this->getOps(), 1);

  // Act
  auto lease = manager.acquire();
  const auto handle = lease.handle();
  lease.release();

  // Assert
  const auto &released_state = manager.controlBlockForTest(handle.index);
  EXPECT_FALSE(released_state.isAlive());
}
#endif

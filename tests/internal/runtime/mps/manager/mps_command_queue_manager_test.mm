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

namespace backend = orteaf::internal::backend;
namespace base = orteaf::internal::base;
namespace diag_error = orteaf::internal::diagnostics::error;
namespace mps_rt = orteaf::internal::runtime::mps::manager;
namespace mps_wrapper = orteaf::internal::runtime::mps::platform::wrapper;
namespace testing_mps = orteaf::tests::runtime::mps::testing;

using orteaf::tests::ExpectError;

namespace {

mps_wrapper::MPSCommandQueue_t makeQueue(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MPSCommandQueue_t>(value);
}

mps_wrapper::MPSEvent_t makeEvent(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MPSEvent_t>(value);
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

TYPED_TEST(MpsCommandQueueManagerTypedTest, GrowthChunkSizeCanBeAdjusted) {
  auto &manager = this->manager();
  EXPECT_EQ(manager.growthChunkSize(), 1u);
  manager.setGrowthChunkSize(4);
  EXPECT_EQ(manager.growthChunkSize(), 4u);
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, GrowthChunkSizeRejectsZero) {
  auto &manager = this->manager();
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
  manager.setGrowthChunkSize(3);
  const auto device = this->adapter().device();
  this->adapter().expectCreateCommandQueues({makeQueue(0x510)});
  manager.initialize(device, this->getOps(), 1);
  auto lease = manager.acquire();
  EXPECT_EQ(manager.growthChunkSize(), 3u);
  manager.release(lease);
  this->adapter().expectDestroyCommandQueues({makeQueue(0x510)});
  manager.shutdown();
}

TYPED_TEST(MpsCommandQueueManagerTypedTest,
           InitializeCreatesConfiguredNumberOfResources) {
  auto &manager = this->manager();
  this->adapter().expectCreateCommandQueues({makeQueue(0x1), makeQueue(0x2)});
  const auto device = this->adapter().device();
  manager.initialize(device, this->getOps(), 2);
  EXPECT_EQ(manager.capacity(), 2u);
  this->adapter().expectDestroyCommandQueuesInOrder(
      {makeQueue(0x1), makeQueue(0x2)});
  manager.shutdown();
}

TYPED_TEST(MpsCommandQueueManagerTypedTest,
           InitializeRejectsCapacityAboveLimit) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
    manager.initialize(device, this->getOps(),
                       std::numeric_limits<std::size_t>::max());
  });
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, CapacityReflectsPoolSize) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  EXPECT_EQ(manager.capacity(), 0u);
  this->adapter().expectCreateCommandQueues(
      {makeQueue(0x301), makeQueue(0x302), makeQueue(0x303)});
  manager.initialize(device, this->getOps(), 3);
  EXPECT_EQ(manager.capacity(), 3u);
  this->adapter().expectDestroyCommandQueues(
      {makeQueue(0x301), makeQueue(0x302), makeQueue(0x303)});
  manager.shutdown();
  EXPECT_EQ(manager.capacity(), 0u);
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, GrowCapacityAddsAdditionalQueues) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  this->adapter().expectCreateCommandQueues({makeQueue(0x350)});
  manager.initialize(device, this->getOps(), 1);
  EXPECT_EQ(manager.capacity(), 1u);

  this->adapter().expectCreateCommandQueues(
      {makeQueue(0x360), makeQueue(0x370)});
  manager.growCapacity(2);
  EXPECT_EQ(manager.capacity(), 3u);

  manager.growCapacity(0);
  EXPECT_EQ(manager.capacity(), 3u);

  auto id0 = manager.acquire();
  auto id1 = manager.acquire();
  auto id2 = manager.acquire();
  EXPECT_NE(id0.handle(), id1.handle());
  EXPECT_NE(id1.handle(), id2.handle());
  EXPECT_NE(id0.handle(), id2.handle());

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

  this->adapter().expectCreateCommandQueues(
      {makeQueue(0x400), makeQueue(0x401)});
  manager.initialize(device, this->getOps(), 2);

  auto lease = manager.acquire();
  lease.release();

  this->adapter().expectDestroyCommandQueues(
      {makeQueue(0x400), makeQueue(0x401)});
  manager.releaseUnusedQueues();

  EXPECT_EQ(manager.capacity(), 0u);

  this->adapter().expectCreateCommandQueues({makeQueue(0x420)});
  auto reacquired = manager.acquire();
  reacquired.release();
  EXPECT_EQ(manager.capacity(), 1u);

  this->adapter().expectDestroyCommandQueues({makeQueue(0x420)});
  manager.shutdown();
  EXPECT_EQ(manager.capacity(), 0u);
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, ManualReleaseInvalidatesLease) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  this->adapter().expectCreateCommandQueues({makeQueue(0x455)});
  manager.initialize(device, this->getOps(), 1);

  auto lease = manager.acquire();
  const auto original_handle = lease.handle();
  ASSERT_TRUE(static_cast<bool>(lease));

  manager.release(lease);                 // manual release wrapper
  EXPECT_FALSE(static_cast<bool>(lease)); // lease should be invalidated

  const auto &state = manager.stateForTest(original_handle.index);
  EXPECT_FALSE(state.in_use);
  EXPECT_GT(state.generation, 0u);

  auto reacquired = manager.acquire();
  EXPECT_NE(reacquired.handle(), original_handle); // generation bumped
  reacquired.release();

  this->adapter().expectDestroyCommandQueues({makeQueue(0x455)});
  manager.shutdown();
}

TYPED_TEST(MpsCommandQueueManagerTypedTest,
           ReleaseUnusedQueuesFailsIfQueuesAreInUse) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  this->adapter().expectCreateCommandQueues(
      {makeQueue(0x430), makeQueue(0x431)});
  manager.initialize(device, this->getOps(), 2);

  auto lease = manager.acquire();
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { manager.releaseUnusedQueues(); });

  manager.release(lease);
  this->adapter().expectDestroyCommandQueues(
      {makeQueue(0x430), makeQueue(0x431)});
  manager.shutdown();
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, AcquireFailsBeforeInitialization) {
  auto &manager = this->manager();
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.acquire(); });
}

TYPED_TEST(MpsCommandQueueManagerTypedTest,
           AcquireReturnsDistinctIdsWithinCapacity) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  this->adapter().expectCreateCommandQueues(
      {makeQueue(0x100), makeQueue(0x200)});
  manager.initialize(device, this->getOps(), 2);
  auto id0 = manager.acquire();
  auto id1 = manager.acquire();
  EXPECT_NE(id0.handle(), id1.handle());
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, AcquireGrowsPoolWhenFreelistEmpty) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  manager.setGrowthChunkSize(1);
  this->adapter().expectCreateCommandQueues({makeQueue(0x300)});
  manager.initialize(device, this->getOps(), 1);
  auto first = manager.acquire();
  this->adapter().expectCreateCommandQueues({makeQueue(0x301)});
  auto second = manager.acquire();
  EXPECT_NE(first.handle(), second.handle());
}

TYPED_TEST(MpsCommandQueueManagerTypedTest,
           AcquireFailsWhenGrowthWouldExceedLimit) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  manager.setGrowthChunkSize(std::numeric_limits<std::size_t>::max());
  this->adapter().expectCreateCommandQueues({makeQueue(0x600)});
  manager.initialize(device, this->getOps(), 1);
  auto lease =
      manager.acquire(); // Keep lease to prevent it from returning to free list
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { (void)manager.acquire(); });
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, ReleaseFailsBeforeInitialization) {
  auto &manager = this->manager();
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.acquire(); });
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, ReleaseRejectsNonAcquiredId) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  this->adapter().expectCreateCommandQueues({makeQueue(0x700)});
  manager.initialize(device, this->getOps(), 1);
  auto lease = manager.acquire();
  auto moved = std::move(lease);
  manager.release(lease); // benign: release moved-from lease does nothing
  manager.release(moved); // actual release
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, ReleaseRequiresCompletedWork) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  this->adapter().expectCreateCommandQueues({makeQueue(0x710)});
  manager.initialize(device, this->getOps(), 1);
  auto lease = manager.acquire();
  lease.release();
}

TYPED_TEST(MpsCommandQueueManagerTypedTest,
           ReleaseMakesHandleStaleAndRecyclesState) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  manager.setGrowthChunkSize(1);
  this->adapter().expectCreateCommandQueues({makeQueue(0x720)});
  manager.initialize(device, this->getOps(), 1);
  auto lease = manager.acquire();
  auto old_handle = lease.handle();
  manager.release(lease);
  this->adapter().expectCreateCommandQueues({});
  auto recycled = manager.acquire();
  EXPECT_NE(recycled.handle(), old_handle);
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, HazardCountersDefaultToZero) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  this->adapter().expectCreateCommandQueues({makeQueue(0x900)});
  manager.initialize(device, this->getOps(), 1);
  auto lease = manager.acquire();
  lease.release();
}

TYPED_TEST(MpsCommandQueueManagerTypedTest,
           HazardCountersCanBeUpdatedAndResetOnRelease) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  this->adapter().expectCreateCommandQueues({makeQueue(0x910)});
  manager.initialize(device, this->getOps(), 1);
  auto lease = manager.acquire();
  lease.release();
  auto recycled = manager.acquire();
  recycled.release();
}

#if ORTEAF_ENABLE_TEST
TYPED_TEST(MpsCommandQueueManagerTypedTest, DebugStateReflectsSetterUpdates) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  this->adapter().expectCreateCommandQueues({makeQueue(0x920)});
  manager.initialize(device, this->getOps(), 1);
  auto lease = manager.acquire();
  const auto handle = lease.handle();
  lease.release();
  const auto &released_state = manager.stateForTest(handle.index);
  EXPECT_FALSE(released_state.in_use);
}
#endif

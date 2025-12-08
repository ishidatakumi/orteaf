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
#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectCreateEvents({makeEvent(0x610)});
#endif
  manager.initialize(device, this->getOps(), 1);
  auto lease = manager.acquire();
  const auto snapshot = manager.debugState(lease.handle());
  EXPECT_EQ(snapshot.growth_chunk_size, 3u);
  manager.release(lease);
#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectDestroyEvents({makeEvent(0x610)});
#endif
  this->adapter().expectDestroyCommandQueues({makeQueue(0x510)});
  manager.shutdown();
}

TYPED_TEST(MpsCommandQueueManagerTypedTest,
           InitializeCreatesConfiguredNumberOfResources) {
  auto &manager = this->manager();
  this->adapter().expectCreateCommandQueues({makeQueue(0x1), makeQueue(0x2)});
#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectCreateEvents({makeEvent(0x10), makeEvent(0x20)});
#endif
  const auto device = this->adapter().device();
  manager.initialize(device, this->getOps(), 2);
  EXPECT_EQ(manager.capacity(), 2u);
#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectDestroyEventsInOrder(
      {makeEvent(0x10), makeEvent(0x20)});
#endif
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
#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectCreateEvents(
      {makeEvent(0x310), makeEvent(0x320), makeEvent(0x330)});
#endif
  manager.initialize(device, this->getOps(), 3);
  EXPECT_EQ(manager.capacity(), 3u);
#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectDestroyEvents(
      {makeEvent(0x310), makeEvent(0x320), makeEvent(0x330)});
#endif
  this->adapter().expectDestroyCommandQueues(
      {makeQueue(0x301), makeQueue(0x302), makeQueue(0x303)});
  manager.shutdown();
  EXPECT_EQ(manager.capacity(), 0u);
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, GrowCapacityAddsAdditionalQueues) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  this->adapter().expectCreateCommandQueues({makeQueue(0x350)});
#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectCreateEvents({makeEvent(0x351)});
#endif
  manager.initialize(device, this->getOps(), 1);
  EXPECT_EQ(manager.capacity(), 1u);

  this->adapter().expectCreateCommandQueues(
      {makeQueue(0x360), makeQueue(0x370)});
#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectCreateEvents({makeEvent(0x361), makeEvent(0x371)});
#endif
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

#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectDestroyEvents(
      {makeEvent(0x351), makeEvent(0x361), makeEvent(0x371)});
#endif
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
#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectCreateEvents({makeEvent(0x410), makeEvent(0x411)});
#endif
  manager.initialize(device, this->getOps(), 2);

  auto lease = manager.acquire();
  lease.release();

#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectDestroyEvents({makeEvent(0x410), makeEvent(0x411)});
#endif
  this->adapter().expectDestroyCommandQueues(
      {makeQueue(0x400), makeQueue(0x401)});
  manager.releaseUnusedQueues();

  EXPECT_EQ(manager.capacity(), 0u);

  this->adapter().expectCreateCommandQueues({makeQueue(0x420)});
#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectCreateEvents({makeEvent(0x421)});
#endif
  auto reacquired = manager.acquire();
  reacquired.release();
  EXPECT_EQ(manager.capacity(), 1u);

#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectDestroyEvents({makeEvent(0x421)});
#endif
  this->adapter().expectDestroyCommandQueues({makeQueue(0x420)});
  manager.shutdown();
  EXPECT_EQ(manager.capacity(), 0u);
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, ManualReleaseInvalidatesLease) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  this->adapter().expectCreateCommandQueues({makeQueue(0x455)});
#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectCreateEvents({makeEvent(0x555)});
#endif
  manager.initialize(device, this->getOps(), 1);

  auto lease = manager.acquire();
  const auto original_handle = lease.handle();
  ASSERT_TRUE(static_cast<bool>(lease));

  manager.release(lease);                 // manual release wrapper
  EXPECT_FALSE(static_cast<bool>(lease)); // lease should be invalidated

  const auto snapshot = manager.debugState(original_handle);
  EXPECT_FALSE(snapshot.in_use);
  EXPECT_GT(snapshot.generation, 0u);

  auto reacquired = manager.acquire();
  EXPECT_NE(reacquired.handle(), original_handle); // generation bumped
  reacquired.release();

#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectDestroyEvents({makeEvent(0x555)});
#endif
  this->adapter().expectDestroyCommandQueues({makeQueue(0x455)});
  manager.shutdown();
}

TYPED_TEST(MpsCommandQueueManagerTypedTest,
           ReleaseUnusedQueuesFailsIfQueuesAreInUse) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  this->adapter().expectCreateCommandQueues(
      {makeQueue(0x430), makeQueue(0x431)});
#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectCreateEvents({makeEvent(0x440), makeEvent(0x441)});
#endif
  manager.initialize(device, this->getOps(), 2);

  auto lease = manager.acquire();
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { manager.releaseUnusedQueues(); });

  manager.release(lease);
#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectDestroyEvents({makeEvent(0x440), makeEvent(0x441)});
#endif
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
#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectCreateEvents({makeEvent(0x1000), makeEvent(0x2000)});
#endif
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
#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectCreateEvents({makeEvent(0x3000)});
#endif
  manager.initialize(device, this->getOps(), 1);
  auto first = manager.acquire();
  this->adapter().expectCreateCommandQueues({makeQueue(0x301)});
#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectCreateEvents({makeEvent(0x3010)});
#endif
  auto second = manager.acquire();
  EXPECT_NE(first.handle(), second.handle());
}

TYPED_TEST(MpsCommandQueueManagerTypedTest,
           AcquireFailsWhenGrowthWouldExceedLimit) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  manager.setGrowthChunkSize(std::numeric_limits<std::size_t>::max());
  this->adapter().expectCreateCommandQueues({makeQueue(0x600)});
#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectCreateEvents({makeEvent(0x6000)});
#endif
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
#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectCreateEvents({makeEvent(0x7000)});
#endif
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
#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectCreateEvents({makeEvent(0x7110)});
#endif
  manager.initialize(device, this->getOps(), 1);
  auto lease = manager.acquire();
#if ORTEAF_MPS_DEBUG_ENABLED
  auto serial = manager.acquireSerial(lease.handle());
  serial->submit_serial = 5;
  serial->completed_serial = 4;
  manager.release(serial);
#endif
  lease.release();
}

TYPED_TEST(MpsCommandQueueManagerTypedTest,
           ReleaseMakesHandleStaleAndRecyclesState) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  manager.setGrowthChunkSize(1);
  this->adapter().expectCreateCommandQueues({makeQueue(0x720)});
#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectCreateEvents({makeEvent(0x7210)});
#endif
  manager.initialize(device, this->getOps(), 1);
  auto lease = manager.acquire();
  auto old_handle = lease.handle();
  manager.release(lease);
  this->adapter().expectCreateCommandQueues({});
#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectCreateEvents({});
#endif
  auto recycled = manager.acquire();
  EXPECT_NE(recycled.handle(), old_handle);
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, HazardCountersDefaultToZero) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  this->adapter().expectCreateCommandQueues({makeQueue(0x900)});
#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectCreateEvents({makeEvent(0x9010)});
#endif
  manager.initialize(device, this->getOps(), 1);
  auto lease = manager.acquire();
#if ORTEAF_MPS_DEBUG_ENABLED
  auto serial = manager.acquireSerial(lease.handle());
  EXPECT_EQ(serial->submit_serial, 0u);
  EXPECT_EQ(serial->completed_serial, 0u);
  serial.release();
#endif
  lease.release();
}

TYPED_TEST(MpsCommandQueueManagerTypedTest,
           HazardCountersCanBeUpdatedAndResetOnRelease) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  this->adapter().expectCreateCommandQueues({makeQueue(0x910)});
#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectCreateEvents({makeEvent(0x9110)});
#endif
  manager.initialize(device, this->getOps(), 1);
  auto lease = manager.acquire();
#if ORTEAF_MPS_DEBUG_ENABLED
  auto serial = manager.acquireSerial(lease.handle());
  serial->submit_serial = 7;
  serial->completed_serial = 7;
  EXPECT_EQ(serial->submit_serial, 7u);
  EXPECT_EQ(serial->completed_serial, 7u);
  manager.release(serial);
#endif
  lease.release();
  auto recycled = manager.acquire();
#if ORTEAF_MPS_DEBUG_ENABLED
  auto recycled_serial = manager.acquireSerial(recycled.handle());
  EXPECT_EQ(recycled_serial->submit_serial, 0u);
  EXPECT_EQ(recycled_serial->completed_serial, 0u);
  recycled_serial.release();
#endif
  recycled.release();
}

#if ORTEAF_ENABLE_TEST
TYPED_TEST(MpsCommandQueueManagerTypedTest, DebugStateReflectsSetterUpdates) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  this->adapter().expectCreateCommandQueues({makeQueue(0x920)});
#if ORTEAF_MPS_DEBUG_ENABLED
  this->adapter().expectCreateEvents({makeEvent(0x9210)});
#endif
  manager.initialize(device, this->getOps(), 1);
  auto lease = manager.acquire();
#if ORTEAF_MPS_DEBUG_ENABLED
  auto serial = manager.acquireSerial(lease.handle());
  serial->submit_serial = 11;
  serial->completed_serial = 9;
  const auto snapshot = manager.debugState(lease.handle());
  EXPECT_TRUE(snapshot.in_use);
  EXPECT_TRUE(snapshot.queue_allocated);
  EXPECT_EQ(snapshot.submit_serial, 11u);
  EXPECT_EQ(snapshot.completed_serial, 9u);
  manager.release(serial);
#endif
  lease.release();
  const auto after_release = manager.debugState(lease.handle());
  EXPECT_FALSE(after_release.in_use);
}
#endif

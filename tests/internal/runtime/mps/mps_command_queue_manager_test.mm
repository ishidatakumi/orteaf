#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include "orteaf/internal/backend/mps/mps_command_queue.h"
#include "orteaf/internal/backend/mps/mps_event.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/runtime/manager/mps/mps_command_queue_manager.h"
#include "tests/internal/runtime/mps/testing/backend_ops_provider.h"
#include "tests/internal/runtime/mps/testing/manager_test_fixture.h"
#include "tests/internal/testing/error_assert.h"

namespace backend = orteaf::internal::backend;
namespace base = orteaf::internal::base;
namespace diag_error = orteaf::internal::diagnostics::error;
namespace mps_rt = orteaf::internal::runtime::mps;
namespace testing_mps = orteaf::tests::runtime::mps::testing;

using orteaf::tests::ExpectError;

namespace {

backend::mps::MPSCommandQueue_t makeQueue(std::uintptr_t value) {
    return reinterpret_cast<backend::mps::MPSCommandQueue_t>(value);
}

backend::mps::MPSEvent_t makeEvent(std::uintptr_t value) {
    return reinterpret_cast<backend::mps::MPSEvent_t>(value);
}

template <class Provider>
class MpsCommandQueueManagerTypedTest
    : public testing_mps::RuntimeManagerFixture<Provider, mps_rt::MpsCommandQueueManager> {
protected:
    using Base = testing_mps::RuntimeManagerFixture<Provider, mps_rt::MpsCommandQueueManager>;

    mps_rt::MpsCommandQueueManager<typename Provider::BackendOps>& manager() {
        return Base::manager();
    }

    auto& adapter() { return Base::adapter(); }
};

#if ORTEAF_ENABLE_MPS
using ProviderTypes = ::testing::Types<
    testing_mps::MockBackendOpsProvider,
    testing_mps::RealBackendOpsProvider>;
#else
using ProviderTypes = ::testing::Types<
    testing_mps::MockBackendOpsProvider>;
#endif

}  // namespace

TYPED_TEST_SUITE(MpsCommandQueueManagerTypedTest, ProviderTypes);

TYPED_TEST(MpsCommandQueueManagerTypedTest, GrowthChunkSizeCanBeAdjusted) {
    auto& manager = this->manager();
    EXPECT_EQ(manager.growthChunkSize(), 1u);
    manager.setGrowthChunkSize(4);
    EXPECT_EQ(manager.growthChunkSize(), 4u);
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, GrowthChunkSizeRejectsZero) {
    auto& manager = this->manager();
    ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] { manager.setGrowthChunkSize(0); });
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, GrowthChunkSizeReflectedInDebugState) {
    if constexpr (!TypeParam::is_mock) {
        GTEST_SKIP() << "Mock-only test";
        return;
    }
    auto& manager = this->manager();
    manager.setGrowthChunkSize(3);
    const auto device = this->adapter().device();
    this->adapter().expectCreateCommandQueues({makeQueue(0x510)});
    this->adapter().expectCreateEvents({makeEvent(0x610)});
    manager.initialize(device, 1);
    const auto id = manager.acquire();
    const auto snapshot = manager.debugState(id);
    EXPECT_EQ(snapshot.growth_chunk_size, 3u);
    manager.release(id);
    this->adapter().expectDestroyEvents({makeEvent(0x610)});
    this->adapter().expectDestroyCommandQueues({makeQueue(0x510)});
    manager.shutdown();
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, InitializeCreatesConfiguredNumberOfResources) {
    auto& manager = this->manager();
    this->adapter().expectCreateCommandQueues({makeQueue(0x1), makeQueue(0x2)});
    this->adapter().expectCreateEvents({makeEvent(0x10), makeEvent(0x20)});
    const auto device = this->adapter().device();
    manager.initialize(device, 2);
    EXPECT_EQ(manager.capacity(), 2u);
    this->adapter().expectDestroyEventsInOrder({makeEvent(0x10), makeEvent(0x20)});
    this->adapter().expectDestroyCommandQueuesInOrder({makeQueue(0x1), makeQueue(0x2)});
    manager.shutdown();
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, InitializeRejectsCapacityAboveLimit) {
    auto& manager = this->manager();
    const auto device = this->adapter().device();
    ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
        manager.initialize(device, std::numeric_limits<std::size_t>::max());
    });
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, CapacityReflectsPoolSize) {
    auto& manager = this->manager();
    const auto device = this->adapter().device();
    EXPECT_EQ(manager.capacity(), 0u);
    this->adapter().expectCreateCommandQueues({makeQueue(0x301), makeQueue(0x302), makeQueue(0x303)});
    this->adapter().expectCreateEvents({makeEvent(0x310), makeEvent(0x320), makeEvent(0x330)});
    manager.initialize(device, 3);
    EXPECT_EQ(manager.capacity(), 3u);
    this->adapter().expectDestroyEvents({makeEvent(0x310), makeEvent(0x320), makeEvent(0x330)});
    this->adapter().expectDestroyCommandQueues({makeQueue(0x301), makeQueue(0x302), makeQueue(0x303)});
    manager.shutdown();
    EXPECT_EQ(manager.capacity(), 0u);
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, GrowCapacityAddsAdditionalQueues) {
    auto& manager = this->manager();
    const auto device = this->adapter().device();
    this->adapter().expectCreateCommandQueues({makeQueue(0x350)});
    this->adapter().expectCreateEvents({makeEvent(0x351)});
    manager.initialize(device, 1);
    EXPECT_EQ(manager.capacity(), 1u);

    this->adapter().expectCreateCommandQueues({makeQueue(0x360), makeQueue(0x370)});
    this->adapter().expectCreateEvents({makeEvent(0x361), makeEvent(0x371)});
    manager.growCapacity(2);
    EXPECT_EQ(manager.capacity(), 3u);

    manager.growCapacity(0);
    EXPECT_EQ(manager.capacity(), 3u);

    const auto id0 = manager.acquire();
    const auto id1 = manager.acquire();
    const auto id2 = manager.acquire();
    EXPECT_NE(id0, id1);
    EXPECT_NE(id1, id2);
    EXPECT_NE(id0, id2);

    manager.release(id0);
    manager.release(id1);
    manager.release(id2);

    this->adapter().expectDestroyEvents({makeEvent(0x351), makeEvent(0x361), makeEvent(0x371)});
    this->adapter().expectDestroyCommandQueues({makeQueue(0x350), makeQueue(0x360), makeQueue(0x370)});
    manager.shutdown();
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, ReleaseUnusedQueuesFreesResourcesAndReallocatesOnDemand) {
    auto& manager = this->manager();
    const auto device = this->adapter().device();

    this->adapter().expectCreateCommandQueues({makeQueue(0x400), makeQueue(0x401)});
    this->adapter().expectCreateEvents({makeEvent(0x410), makeEvent(0x411)});
    manager.initialize(device, 2);

    const auto id = manager.acquire();
    manager.release(id);

    this->adapter().expectDestroyEvents({makeEvent(0x410), makeEvent(0x411)});
    this->adapter().expectDestroyCommandQueues({makeQueue(0x400), makeQueue(0x401)});
    manager.releaseUnusedQueues();

    EXPECT_EQ(manager.capacity(), 0u);

    this->adapter().expectCreateCommandQueues({makeQueue(0x420)});
    this->adapter().expectCreateEvents({makeEvent(0x421)});
    const auto reacquired = manager.acquire();
    manager.release(reacquired);
    EXPECT_EQ(manager.capacity(), 1u);

    this->adapter().expectDestroyEvents({makeEvent(0x421)});
    this->adapter().expectDestroyCommandQueues({makeQueue(0x420)});
    manager.shutdown();
    EXPECT_EQ(manager.capacity(), 0u);
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, ReleaseUnusedQueuesFailsIfQueuesAreInUse) {
    auto& manager = this->manager();
    const auto device = this->adapter().device();

    this->adapter().expectCreateCommandQueues({makeQueue(0x430), makeQueue(0x431)});
    this->adapter().expectCreateEvents({makeEvent(0x440), makeEvent(0x441)});
    manager.initialize(device, 2);

    const auto id = manager.acquire();
    ExpectError(diag_error::OrteafErrc::InvalidState, [&] { manager.releaseUnusedQueues(); });

    manager.release(id);
    this->adapter().expectDestroyEvents({makeEvent(0x440), makeEvent(0x441)});
    this->adapter().expectDestroyCommandQueues({makeQueue(0x430), makeQueue(0x431)});
    manager.shutdown();
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, AcquireFailsBeforeInitialization) {
    auto& manager = this->manager();
    ExpectError(diag_error::OrteafErrc::InvalidState, [&] { (void)manager.acquire(); });
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, AcquireReturnsDistinctIdsWithinCapacity) {
    auto& manager = this->manager();
    const auto device = this->adapter().device();
    this->adapter().expectCreateCommandQueues({makeQueue(0x100), makeQueue(0x200)});
    this->adapter().expectCreateEvents({makeEvent(0x1000), makeEvent(0x2000)});
    manager.initialize(device, 2);
    const auto id0 = manager.acquire();
    const auto id1 = manager.acquire();
    EXPECT_NE(id0, id1);
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, AcquireGrowsPoolWhenFreelistEmpty) {
    auto& manager = this->manager();
    const auto device = this->adapter().device();
    manager.setGrowthChunkSize(1);
    this->adapter().expectCreateCommandQueues({makeQueue(0x300)});
    this->adapter().expectCreateEvents({makeEvent(0x3000)});
    manager.initialize(device, 1);
    const auto first = manager.acquire();
    this->adapter().expectCreateCommandQueues({makeQueue(0x301)});
    this->adapter().expectCreateEvents({makeEvent(0x3010)});
    const auto second = manager.acquire();
    EXPECT_NE(first, second);
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, AcquireFailsWhenGrowthWouldExceedLimit) {
    auto& manager = this->manager();
    const auto device = this->adapter().device();
    manager.setGrowthChunkSize(std::numeric_limits<std::size_t>::max());
    this->adapter().expectCreateCommandQueues({makeQueue(0x600)});
    this->adapter().expectCreateEvents({makeEvent(0x6000)});
    manager.initialize(device, 1);
    (void)manager.acquire();
    ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] { (void)manager.acquire(); });
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, ReleaseFailsBeforeInitialization) {
    auto& manager = this->manager();
    ExpectError(diag_error::OrteafErrc::InvalidState, [&] { manager.release(base::CommandQueueId{0}); });
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, ReleaseRejectsNonAcquiredId) {
    auto& manager = this->manager();
    const auto device = this->adapter().device();
    this->adapter().expectCreateCommandQueues({makeQueue(0x700)});
    this->adapter().expectCreateEvents({makeEvent(0x7000)});
    manager.initialize(device, 1);
    ExpectError(diag_error::OrteafErrc::InvalidState, [&] { manager.release(base::CommandQueueId{0}); });
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, ReleaseRequiresCompletedWork) {
    auto& manager = this->manager();
    const auto device = this->adapter().device();
    this->adapter().expectCreateCommandQueues({makeQueue(0x710)});
    this->adapter().expectCreateEvents({makeEvent(0x7110)});
    manager.initialize(device, 1);
    const auto id = manager.acquire();
    manager.setSubmitSerial(id, 5);
    manager.setCompletedSerial(id, 4);
    ExpectError(diag_error::OrteafErrc::InvalidState, [&] { manager.release(id); });
    manager.setCompletedSerial(id, 5);
    EXPECT_NO_THROW(manager.release(id));
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, ReleaseMakesHandleStaleAndRecyclesState) {
    auto& manager = this->manager();
    const auto device = this->adapter().device();
    manager.setGrowthChunkSize(1);
    this->adapter().expectCreateCommandQueues({makeQueue(0x720)});
    this->adapter().expectCreateEvents({makeEvent(0x7210)});
    manager.initialize(device, 1);
    const auto id = manager.acquire();
    manager.release(id);
    ExpectError(diag_error::OrteafErrc::InvalidState, [&] { manager.release(id); });
    this->adapter().expectCreateCommandQueues({});
    this->adapter().expectCreateEvents({});
    const auto recycled = manager.acquire();
    EXPECT_NE(recycled, id);
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, GetCommandQueueFailsBeforeInitialization) {
    auto& manager = this->manager();
    ExpectError(diag_error::OrteafErrc::InvalidState, [&] { (void)manager.getCommandQueue(base::CommandQueueId{0}); });
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, HazardCountersDefaultToZero) {
    auto& manager = this->manager();
    const auto device = this->adapter().device();
    this->adapter().expectCreateCommandQueues({makeQueue(0x900)});
    this->adapter().expectCreateEvents({makeEvent(0x9010)});
    manager.initialize(device, 1);
    const auto id = manager.acquire();
    EXPECT_EQ(manager.submitSerial(id), 0u);
    EXPECT_EQ(manager.completedSerial(id), 0u);
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, HazardCountersCanBeUpdatedAndResetOnRelease) {
    auto& manager = this->manager();
    const auto device = this->adapter().device();
    this->adapter().expectCreateCommandQueues({makeQueue(0x910)});
    this->adapter().expectCreateEvents({makeEvent(0x9110)});
    manager.initialize(device, 1);
    const auto id = manager.acquire();
    manager.setSubmitSerial(id, 7);
    manager.setCompletedSerial(id, 7);
    EXPECT_EQ(manager.submitSerial(id), 7u);
    EXPECT_EQ(manager.completedSerial(id), 7u);
    manager.release(id);
    const auto recycled = manager.acquire();
    EXPECT_EQ(manager.submitSerial(recycled), 0u);
    EXPECT_EQ(manager.completedSerial(recycled), 0u);
}

#if ORTEAF_ENABLE_TEST
TYPED_TEST(MpsCommandQueueManagerTypedTest, DebugStateReflectsSetterUpdates) {
    auto& manager = this->manager();
    const auto device = this->adapter().device();
    this->adapter().expectCreateCommandQueues({makeQueue(0x920)});
    this->adapter().expectCreateEvents({makeEvent(0x9210)});
    manager.initialize(device, 1);
    const auto id = manager.acquire();
    manager.setSubmitSerial(id, 11);
    manager.setCompletedSerial(id, 9);
    const auto snapshot = manager.debugState(id);
    EXPECT_TRUE(snapshot.in_use);
    EXPECT_TRUE(snapshot.queue_allocated);
    EXPECT_EQ(snapshot.submit_serial, 11u);
    EXPECT_EQ(snapshot.completed_serial, 9u);
    manager.setCompletedSerial(id, 11);
    manager.release(id);
    const auto after_release = manager.debugState(id);
    EXPECT_FALSE(after_release.in_use);
}
#endif

TYPED_TEST(MpsCommandQueueManagerTypedTest, GetCommandQueueReturnsHandleForAcquiredId) {
    auto& manager = this->manager();
    const auto device = this->adapter().device();
    this->adapter().expectCreateCommandQueues({makeQueue(0x800)});
    this->adapter().expectCreateEvents({makeEvent(0x8000)});
    manager.initialize(device, 1);
    const auto id = manager.acquire();
    const auto queue = manager.getCommandQueue(id);
    if constexpr (TypeParam::is_mock) {
        EXPECT_EQ(queue, makeQueue(0x800));
    } else {
        EXPECT_NE(queue, nullptr);
    }
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, GetCommandQueueRejectsOutOfRangeId) {
    auto& manager = this->manager();
    const auto device = this->adapter().device();
    this->adapter().expectCreateCommandQueues({makeQueue(0x810)});
    this->adapter().expectCreateEvents({makeEvent(0x8110)});
    manager.initialize(device, 1);
    ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
        (void)manager.getCommandQueue(base::CommandQueueId{10});
    });
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, GetCommandQueueRejectsStaleId) {
    auto& manager = this->manager();
    const auto device = this->adapter().device();
    manager.setGrowthChunkSize(1);
    this->adapter().expectCreateCommandQueues({makeQueue(0x820)});
    this->adapter().expectCreateEvents({makeEvent(0x8210)});
    manager.initialize(device, 1);
    const auto id = manager.acquire();
    manager.release(id);
    ExpectError(diag_error::OrteafErrc::InvalidState, [&] { (void)manager.getCommandQueue(id); });
}

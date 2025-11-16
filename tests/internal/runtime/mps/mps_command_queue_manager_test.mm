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

TEST_F(MpsCommandQueueManagerTest, GrowthChunkSizeCanBeAdjusted) {
    EXPECT_EQ(manager_.growthChunkSize(), 1u);
    manager_.setGrowthChunkSize(4);
    EXPECT_EQ(manager_.growthChunkSize(), 4u);
}

TEST_F(MpsCommandQueueManagerTest, GrowthChunkSizeRejectsZero) {
    ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
        manager_.setGrowthChunkSize(0);
    });
}

// -----------------------------------------------------------------------------
// initialize / shutdown basics
// -----------------------------------------------------------------------------

TEST_F(MpsCommandQueueManagerTest, InitializeCreatesConfiguredNumberOfResources) {
    EXPECT_CALL(mock_, createCommandQueue(_)).WillOnce(Return(makeQueue(0x1))).WillOnce(Return(makeQueue(0x2)));
    EXPECT_CALL(mock_, createEvent(_)).WillOnce(Return(makeEvent(0x10))).WillOnce(Return(makeEvent(0x20)));

    manager_.initialize(2);
    EXPECT_EQ(manager_.capacity(), 2u);

    {
        InSequence seq;
        EXPECT_CALL(mock_, destroyEvent(makeEvent(0x10)));
        EXPECT_CALL(mock_, destroyCommandQueue(makeQueue(0x1)));
        EXPECT_CALL(mock_, destroyEvent(makeEvent(0x20)));
        EXPECT_CALL(mock_, destroyCommandQueue(makeQueue(0x2)));
    }
}

TEST_F(MpsCommandQueueManagerTest, InitializeRejectsCapacityAboveLimit) {
    ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
        manager_.initialize(std::numeric_limits<std::size_t>::max());
    });
}

TEST_F(MpsCommandQueueManagerTest, CapacityReflectsPoolSize) {
    EXPECT_EQ(manager_.capacity(), 0u);

    EXPECT_CALL(mock_, createCommandQueue(_)).WillRepeatedly(Return(makeQueue(0x3)));
    EXPECT_CALL(mock_, createEvent(_)).WillRepeatedly(Return(makeEvent(0x30)));
    manager_.initialize(3);
    EXPECT_EQ(manager_.capacity(), 3u);

    EXPECT_CALL(mock_, destroyEvent(_)).Times(3);
    EXPECT_CALL(mock_, destroyCommandQueue(_)).Times(3);
    manager_.shutdown();
    EXPECT_EQ(manager_.capacity(), 0u);
}

// -----------------------------------------------------------------------------
// acquire basics
// -----------------------------------------------------------------------------
TEST_F(MpsCommandQueueManagerTest, AcquireFailsBeforeInitialization) {
    ExpectError(diag_error::OrteafErrc::InvalidState, [&] {
        (void)manager_.acquire();
    });
}

TEST_F(MpsCommandQueueManagerTest, AcquireReturnsDistinctIdsWithinCapacity) {
    EXPECT_CALL(mock_, createCommandQueue(_)).WillOnce(Return(makeQueue(0x100))).WillOnce(Return(makeQueue(0x200)));
    EXPECT_CALL(mock_, createEvent(_)).WillOnce(Return(makeEvent(0x1000))).WillOnce(Return(makeEvent(0x2000)));

    manager_.initialize(2);

    const auto id0 = manager_.acquire();
    const auto id1 = manager_.acquire();
    EXPECT_NE(id0, id1);
}

TEST_F(MpsCommandQueueManagerTest, AcquireGrowsPoolWhenFreelistEmpty) {
    manager_.setGrowthChunkSize(1);
    EXPECT_CALL(mock_, createCommandQueue(_)).WillOnce(Return(makeQueue(0x300)));
    EXPECT_CALL(mock_, createEvent(_)).WillOnce(Return(makeEvent(0x3000)));
    manager_.initialize(1);

    const auto first = manager_.acquire();

    EXPECT_CALL(mock_, createCommandQueue(_)).WillOnce(Return(makeQueue(0x301)));
    EXPECT_CALL(mock_, createEvent(_)).WillOnce(Return(makeEvent(0x3010)));
    const auto second = manager_.acquire();
    EXPECT_NE(first, second);
}

TEST_F(MpsCommandQueueManagerTest, AcquireFailsWhenGrowthWouldExceedLimit) {
    manager_.setGrowthChunkSize(std::numeric_limits<std::size_t>::max());
    EXPECT_CALL(mock_, createCommandQueue(_)).WillOnce(Return(makeQueue(0x600)));
    EXPECT_CALL(mock_, createEvent(_)).WillOnce(Return(makeEvent(0x6000)));
    manager_.initialize(1);

    const auto first = manager_.acquire();
    (void)first;

    ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
        (void)manager_.acquire();
    });
}

// -----------------------------------------------------------------------------
// release basics
// -----------------------------------------------------------------------------
TEST_F(MpsCommandQueueManagerTest, ReleaseFailsBeforeInitialization) {
    ExpectError(diag_error::OrteafErrc::InvalidState, [&] {
        manager_.release(base::CommandQueueId{0});
    });
}

TEST_F(MpsCommandQueueManagerTest, ReleaseRejectsNonAcquiredId) {
    EXPECT_CALL(mock_, createCommandQueue(_)).WillOnce(Return(makeQueue(0x700)));
    EXPECT_CALL(mock_, createEvent(_)).WillOnce(Return(makeEvent(0x7000)));
    manager_.initialize(1);

    ExpectError(diag_error::OrteafErrc::InvalidState, [&] {
        manager_.release(base::CommandQueueId{0});
    });
}

TEST_F(MpsCommandQueueManagerTest, ReleaseRequiresCompletedWork) {
    EXPECT_CALL(mock_, createCommandQueue(_)).WillOnce(Return(makeQueue(0x710)));
    EXPECT_CALL(mock_, createEvent(_)).WillOnce(Return(makeEvent(0x7110)));
    manager_.initialize(1);

    const auto id = manager_.acquire();
    manager_.setSubmitSerial(id, 5);
    manager_.setCompletedSerial(id, 4);
    ExpectError(diag_error::OrteafErrc::InvalidState, [&] {
        manager_.release(id);
    });

    manager_.setCompletedSerial(id, 5);
    EXPECT_NO_THROW(manager_.release(id));
}

TEST_F(MpsCommandQueueManagerTest, ReleaseMakesHandleStaleAndRecyclesState) {
    manager_.setGrowthChunkSize(1);
    EXPECT_CALL(mock_, createCommandQueue(_)).WillOnce(Return(makeQueue(0x720)));
    EXPECT_CALL(mock_, createEvent(_)).WillOnce(Return(makeEvent(0x7210)));
    manager_.initialize(1);

    const auto id = manager_.acquire();
    manager_.release(id);

    ExpectError(diag_error::OrteafErrc::InvalidState, [&] {
        manager_.release(id);
    });

    EXPECT_CALL(mock_, createCommandQueue(_)).Times(0);
    EXPECT_CALL(mock_, createEvent(_)).Times(0);
    const auto recycled = manager_.acquire();
    EXPECT_NE(recycled, id);
}

// -----------------------------------------------------------------------------
// getCommandQueue basics
// -----------------------------------------------------------------------------
TEST_F(MpsCommandQueueManagerTest, GetCommandQueueFailsBeforeInitialization) {
    ExpectError(diag_error::OrteafErrc::InvalidState, [&] {
        (void)manager_.getCommandQueue(base::CommandQueueId{0});
    });
}

// -----------------------------------------------------------------------------
// Hazard counters (submit/completed serial)
// -----------------------------------------------------------------------------
TEST_F(MpsCommandQueueManagerTest, HazardCountersDefaultToZero) {
    EXPECT_CALL(mock_, createCommandQueue(_)).WillOnce(Return(makeQueue(0x900)));
    EXPECT_CALL(mock_, createEvent(_)).WillOnce(Return(makeEvent(0x9010)));
    manager_.initialize(1);

    const auto id = manager_.acquire();
    EXPECT_EQ(manager_.submitSerial(id), 0u);
    EXPECT_EQ(manager_.completedSerial(id), 0u);
}

TEST_F(MpsCommandQueueManagerTest, HazardCountersCanBeUpdatedAndResetOnRelease) {
    EXPECT_CALL(mock_, createCommandQueue(_)).WillOnce(Return(makeQueue(0x910)));
    EXPECT_CALL(mock_, createEvent(_)).WillOnce(Return(makeEvent(0x9110)));
    manager_.initialize(1);

    const auto id = manager_.acquire();
    manager_.setSubmitSerial(id, 7);
    manager_.setCompletedSerial(id, 7);
    EXPECT_EQ(manager_.submitSerial(id), 7u);
    EXPECT_EQ(manager_.completedSerial(id), 7u);

    manager_.release(id);

    const auto recycled = manager_.acquire();
    EXPECT_EQ(manager_.submitSerial(recycled), 0u);
    EXPECT_EQ(manager_.completedSerial(recycled), 0u);
}

TEST_F(MpsCommandQueueManagerTest, DebugStateReflectsSetterUpdates) {
    EXPECT_CALL(mock_, createCommandQueue(_)).WillOnce(Return(makeQueue(0x920)));
    EXPECT_CALL(mock_, createEvent(_)).WillOnce(Return(makeEvent(0x9210)));
    manager_.initialize(1);

    const auto id = manager_.acquire();
    manager_.setSubmitSerial(id, 11);
    manager_.setCompletedSerial(id, 9);

    const auto snapshot = manager_.debugState(id);
    EXPECT_TRUE(snapshot.in_use);
    EXPECT_TRUE(snapshot.queue_allocated);
    EXPECT_EQ(snapshot.submit_serial, 11u);
    EXPECT_EQ(snapshot.completed_serial, 9u);
    EXPECT_NE(snapshot.generation, std::numeric_limits<std::uint32_t>::max());

    manager_.setCompletedSerial(id, 11);
    manager_.release(id);
    const auto after_release = manager_.debugState(id);
    EXPECT_FALSE(after_release.in_use);
}

TEST_F(MpsCommandQueueManagerTest, GetCommandQueueReturnsHandleForAcquiredId) {
    EXPECT_CALL(mock_, createCommandQueue(_)).WillOnce(Return(makeQueue(0x800)));
    EXPECT_CALL(mock_, createEvent(_)).WillOnce(Return(makeEvent(0x8000)));
    manager_.initialize(1);

    const auto id = manager_.acquire();
    EXPECT_EQ(manager_.getCommandQueue(id), makeQueue(0x800));
}

TEST_F(MpsCommandQueueManagerTest, GetCommandQueueRejectsOutOfRangeId) {
    EXPECT_CALL(mock_, createCommandQueue(_)).WillOnce(Return(makeQueue(0x810)));
    EXPECT_CALL(mock_, createEvent(_)).WillOnce(Return(makeEvent(0x8110)));
    manager_.initialize(1);

    ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
        (void)manager_.getCommandQueue(base::CommandQueueId{10});
    });
}

TEST_F(MpsCommandQueueManagerTest, GetCommandQueueRejectsStaleId) {
    manager_.setGrowthChunkSize(1);
    EXPECT_CALL(mock_, createCommandQueue(_)).WillOnce(Return(makeQueue(0x820)));
    EXPECT_CALL(mock_, createEvent(_)).WillOnce(Return(makeEvent(0x8210)));
    manager_.initialize(1);

    const auto id = manager_.acquire();
    manager_.release(id);

    ExpectError(diag_error::OrteafErrc::InvalidState, [&] {
        (void)manager_.getCommandQueue(id);
    });
}

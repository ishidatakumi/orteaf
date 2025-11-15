#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <limits>

#include "orteaf/internal/backend/mps/mps_command_queue.h"
#include "orteaf/internal/backend/mps/mps_event.h"
#include "orteaf/internal/runtime/manager/mps/mps_command_queue_manager.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "tests/internal/runtime/mps/testing/backend_mock.h"
#include "tests/internal/testing/error_assert.h"

namespace backend = orteaf::internal::backend;
namespace base = orteaf::internal::base;
namespace mps_rt = orteaf::internal::runtime::mps;
namespace test_mps = orteaf::tests::runtime::mps;

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::InSequence;
using ::testing::_;
namespace diag_error = orteaf::internal::diagnostics::error;
using orteaf::tests::ExpectError;

namespace {

backend::mps::MPSCommandQueue_t makeQueue(std::uintptr_t value) {
    return reinterpret_cast<backend::mps::MPSCommandQueue_t>(value);
}

backend::mps::MPSEvent_t makeEvent(std::uintptr_t value) {
    return reinterpret_cast<backend::mps::MPSEvent_t>(value);
}

using MockManager = mps_rt::MpsCommandQueueManager<test_mps::MpsBackendOpsMockAdapter>;

class MpsCommandQueueManagerTest : public ::testing::Test {
protected:
    MpsCommandQueueManagerTest()
        : guard_(mock_) {}

    void TearDown() override {
        manager_.shutdown();
        test_mps::MpsBackendOpsMockRegistry::unbind(mock_);
    }

    NiceMock<test_mps::MpsBackendOpsMock> mock_;
    test_mps::MpsBackendOpsMockRegistry::Guard guard_;
    MockManager manager_;
};

}  // namespace

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

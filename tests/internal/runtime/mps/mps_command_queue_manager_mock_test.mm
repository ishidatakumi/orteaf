#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <system_error>

#include "orteaf/internal/runtime/manager/mps/mps_command_queue_manager.h"
#include "orteaf/internal/backend/mps/mps_command_queue.h"
#include "orteaf/internal/backend/mps/mps_event.h"
#include "orteaf/internal/base/strong_id.h"
#include "tests/internal/runtime/mps/testing/backend_mock.h"

namespace backend = orteaf::internal::backend;
namespace base = orteaf::internal::base;
namespace mps_rt = orteaf::internal::runtime::mps;
namespace test_mps = orteaf::tests::runtime::mps;

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::UnorderedElementsAre;
using ::testing::StrictMock;

namespace {

using MockMpsCommandQueueManager = mps_rt::MpsCommandQueueManager<test_mps::MpsBackendOpsMockAdapter>;

backend::mps::MPSCommandQueue_t makeFakeCommandQueue(std::uintptr_t value) {
    return reinterpret_cast<backend::mps::MPSCommandQueue_t>(value);
}

backend::mps::MPSEvent_t makeFakeEvent(std::uintptr_t value) {
    return reinterpret_cast<backend::mps::MPSEvent_t>(value);
}

}  // namespace

class MpsCommandQueueManagerMockTest : public ::testing::Test {
protected:
    MpsCommandQueueManagerMockTest()
        : guard_(mock_) {}

    void TearDown() override {
        manager_.shutdown();
        test_mps::MpsBackendOpsMockRegistry::unbind(mock_);
    }

    NiceMock<test_mps::MpsBackendOpsMock> mock_;
    test_mps::MpsBackendOpsMockRegistry::Guard guard_;
    MockMpsCommandQueueManager manager_;

    struct QueueResource {
        backend::mps::MPSCommandQueue_t queue;
        backend::mps::MPSEvent_t event;
    };

    template <std::size_t N>
    void expectCreateResources(const std::array<QueueResource, N>& resources) {
        for (const auto& resource : resources) {
            EXPECT_CALL(mock_, createCommandQueue(_)).WillOnce(Return(resource.queue));
            EXPECT_CALL(mock_, createEvent(_)).WillOnce(Return(resource.event));
        }
    }

    template <std::size_t N>
    void expectDestroyResources(const std::array<QueueResource, N>& resources) {
        for (const auto& resource : resources) {
            EXPECT_CALL(mock_, destroyEvent(resource.event));
            EXPECT_CALL(mock_, destroyCommandQueue(resource.queue));
        }
    }
};

TEST_F(MpsCommandQueueManagerMockTest, InitializeCreatesCommandQueuesAndEvents) {
    const std::array<QueueResource, 2> resources{{
        {makeFakeCommandQueue(0x1), makeFakeEvent(0x10)},
        {makeFakeCommandQueue(0x2), makeFakeEvent(0x20)},
    }};

    {
        ::testing::InSequence seq;
        expectCreateResources(resources);
    }

    manager_.initialize(resources.size());
    EXPECT_EQ(manager_.capacity(), resources.size());

    expectDestroyResources(resources);
}

TEST_F(MpsCommandQueueManagerMockTest, ZeroCapacityStillMarksInitialized) {
    EXPECT_CALL(mock_, createCommandQueue(_)).Times(0);
    EXPECT_CALL(mock_, createEvent(_)).Times(0);

    manager_.initialize(0);
    EXPECT_EQ(manager_.capacity(), 0u);
    EXPECT_THROW(manager_.acquire(), std::system_error);
}

TEST_F(MpsCommandQueueManagerMockTest, ReinitializeDestroysPreviousQueues) {
    const std::array<QueueResource, 2> first_resources{{
        {makeFakeCommandQueue(0x3), makeFakeEvent(0x30)},
        {makeFakeCommandQueue(0x4), makeFakeEvent(0x40)},
    }};
    const std::array<QueueResource, 2> second_resources{{
        {makeFakeCommandQueue(0x5), makeFakeEvent(0x50)},
        {makeFakeCommandQueue(0x6), makeFakeEvent(0x60)},
    }};

    expectCreateResources(first_resources);
    manager_.initialize(first_resources.size());

    {
        ::testing::InSequence seq;
        expectDestroyResources(first_resources);
        expectCreateResources(second_resources);
    }
    manager_.initialize(second_resources.size());
    EXPECT_EQ(manager_.capacity(), second_resources.size());

    expectDestroyResources(second_resources);
}

TEST_F(MpsCommandQueueManagerMockTest, AccessBeforeInitializationThrows) {
    EXPECT_THROW(manager_.getCommandQueue(base::CommandQueueId{0}), std::system_error);
    EXPECT_THROW(manager_.acquire(), std::system_error);
    EXPECT_THROW(manager_.release(base::CommandQueueId{0}), std::system_error);
}

TEST_F(MpsCommandQueueManagerMockTest, AcquireReturnsUniqueQueuesUntilCapacity) {
    const std::array<QueueResource, 2> resources{{
        {makeFakeCommandQueue(0x10), makeFakeEvent(0x100)},
        {makeFakeCommandQueue(0x11), makeFakeEvent(0x110)},
    }};
    expectCreateResources(resources);
    manager_.initialize(resources.size());

    const auto id0 = manager_.acquire();
    const auto id1 = manager_.acquire();
    EXPECT_NE(id0, id1);

    const std::array<backend::mps::MPSCommandQueue_t, 2> queues{
        manager_.getCommandQueue(id0),
        manager_.getCommandQueue(id1),
    };
    EXPECT_THAT(queues, UnorderedElementsAre(resources[0].queue, resources[1].queue));
}

TEST_F(MpsCommandQueueManagerMockTest, AcquireBeyondCapacityThrows) {
    const std::array<QueueResource, 1> resources{{
        {makeFakeCommandQueue(0x21), makeFakeEvent(0x210)},
    }};
    expectCreateResources(resources);
    manager_.initialize(resources.size());

    const auto id = manager_.acquire();
    EXPECT_THROW(manager_.acquire(), std::system_error);
    manager_.release(id);
}

TEST_F(MpsCommandQueueManagerMockTest, ReleaseReturnsQueueToPool) {
    const std::array<QueueResource, 1> resources{{
        {makeFakeCommandQueue(0x30), makeFakeEvent(0x300)},
    }};
    expectCreateResources(resources);
    manager_.initialize(resources.size());

    const auto first_id = manager_.acquire();
    const auto queue_handle = manager_.getCommandQueue(first_id);
    manager_.release(first_id);
    EXPECT_THROW(manager_.getCommandQueue(first_id), std::system_error);

    const auto recycled_id = manager_.acquire();
    EXPECT_NE(first_id, recycled_id);
    EXPECT_EQ(manager_.getCommandQueue(recycled_id), queue_handle);
    manager_.release(recycled_id);
}

TEST_F(MpsCommandQueueManagerMockTest, StateAccessorsExposeHazardCounters) {
    const std::array<QueueResource, 1> resources{{
        {makeFakeCommandQueue(0x33), makeFakeEvent(0x330)},
    }};
    expectCreateResources(resources);
    manager_.initialize(resources.size());

    const auto id = manager_.acquire();
    EXPECT_EQ(manager_.submitSerial(id), 0u);
    EXPECT_EQ(manager_.completedSerial(id), 0u);

    manager_.setSubmitSerial(id, 5);
    manager_.setCompletedSerial(id, 2);

    EXPECT_EQ(manager_.submitSerial(id), 5u);
    EXPECT_EQ(manager_.completedSerial(id), 2u);

    manager_.release(id);
}

TEST_F(MpsCommandQueueManagerMockTest, ReleaseWithInvalidIdThrows) {
    const std::array<QueueResource, 1> resources{{
        {makeFakeCommandQueue(0x40), makeFakeEvent(0x400)},
    }};
    expectCreateResources(resources);
    manager_.initialize(resources.size());

    EXPECT_THROW(manager_.release(base::CommandQueueId{1234}), std::system_error);
}

TEST_F(MpsCommandQueueManagerMockTest, StaleIdCannotBeUsedAfterRelease) {
    const std::array<QueueResource, 1> resources{{
        {makeFakeCommandQueue(0x50), makeFakeEvent(0x500)},
    }};
    expectCreateResources(resources);
    manager_.initialize(resources.size());

    const auto id = manager_.acquire();
    manager_.release(id);
    EXPECT_THROW(manager_.release(id), std::system_error);
    EXPECT_THROW(manager_.getCommandQueue(id), std::system_error);

    const auto refreshed = manager_.acquire();
    EXPECT_NE(refreshed, id);
    EXPECT_NO_THROW(manager_.getCommandQueue(refreshed));
    manager_.release(refreshed);
}

TEST_F(MpsCommandQueueManagerMockTest, ShutdownDestroysAllResources) {
    const std::array<QueueResource, 2> resources{{
        {makeFakeCommandQueue(0x60), makeFakeEvent(0x600)},
        {makeFakeCommandQueue(0x61), makeFakeEvent(0x610)},
    }};
    expectCreateResources(resources);
    manager_.initialize(resources.size());

    expectDestroyResources(resources);
    manager_.shutdown();
    EXPECT_EQ(manager_.capacity(), 0u);
}

/**
 * @file mps_reuse_ticket_test.mm
 * @brief Tests for MpsReuseTicket.
 */

#import <Metal/Metal.h>

#include <gtest/gtest.h>
#include <type_traits>

#include "orteaf/internal/backend/mps/mps_reuse_ticket.h"
#include "orteaf/internal/backend/mps/wrapper/mps_command_buffer.h"
#include "orteaf/internal/backend/mps/wrapper/mps_command_queue.h"
#include "orteaf/internal/backend/mps/wrapper/mps_device.h"

namespace mps_backend = orteaf::internal::backend::mps;
namespace base = orteaf::internal::base;

class MpsReuseTicketTest : public ::testing::Test {
protected:
    void SetUp() override {
#if ORTEAF_ENABLE_MPS
        device_ = mps_backend::getDevice();
        if (device_ == nullptr) {
            GTEST_SKIP() << "No Metal devices available";
        }
        command_queue_ = mps_backend::createCommandQueue(device_);
        if (command_queue_ == nullptr) {
            GTEST_SKIP() << "Failed to create command queue";
        }
        command_buffer_ = mps_backend::createCommandBuffer(command_queue_);
        if (command_buffer_ == nullptr) {
            GTEST_SKIP() << "Failed to create command buffer";
        }
#else
        GTEST_SKIP() << "MPS not enabled";
#endif
    }

    void TearDown() override {
#if ORTEAF_ENABLE_MPS
        if (command_buffer_ != nullptr) {
            mps_backend::destroyCommandBuffer(command_buffer_);
            command_buffer_ = nullptr;
        }
        if (command_queue_ != nullptr) {
            mps_backend::destroyCommandQueue(command_queue_);
            command_queue_ = nullptr;
        }
        if (device_ != nullptr) {
            mps_backend::deviceRelease(device_);
            device_ = nullptr;
        }
#endif
    }

#if ORTEAF_ENABLE_MPS
    mps_backend::MPSDevice_t device_{nullptr};
    mps_backend::MPSCommandQueue_t command_queue_{nullptr};
    mps_backend::MPSCommandBuffer_t command_buffer_{nullptr};
    base::CommandQueueId queue_id_{base::CommandQueueId{9}};
#endif
};

#if ORTEAF_ENABLE_MPS

TEST_F(MpsReuseTicketTest, DefaultConstructedIsInvalid) {
    mps_backend::MpsReuseTicket ticket;
    EXPECT_FALSE(ticket.valid());
    EXPECT_EQ(ticket.commandQueueId(), base::CommandQueueId{});
    EXPECT_EQ(ticket.commandBuffer(), nullptr);
}

TEST_F(MpsReuseTicketTest, StoresMembersFromConstructor) {
    mps_backend::MpsReuseTicket ticket(queue_id_, command_buffer_);
    EXPECT_TRUE(ticket.valid());
    EXPECT_EQ(ticket.commandQueueId(), queue_id_);
    EXPECT_EQ(ticket.commandBuffer(), command_buffer_);
}

TEST_F(MpsReuseTicketTest, SettersAndMoveTransferState) {
    mps_backend::MpsReuseTicket ticket;
    ticket.setCommandQueueId(queue_id_).setCommandBuffer(command_buffer_);

    EXPECT_TRUE(ticket.valid());
    EXPECT_EQ(ticket.commandQueueId(), queue_id_);
    EXPECT_EQ(ticket.commandBuffer(), command_buffer_);

    mps_backend::MpsReuseTicket moved(std::move(ticket));
    EXPECT_TRUE(moved.valid());
    EXPECT_EQ(moved.commandQueueId(), queue_id_);
    EXPECT_EQ(moved.commandBuffer(), command_buffer_);

    EXPECT_FALSE(ticket.valid());
    EXPECT_EQ(ticket.commandBuffer(), nullptr);
}

static_assert(!std::is_copy_constructible_v<mps_backend::MpsReuseTicket>);
static_assert(!std::is_copy_assignable_v<mps_backend::MpsReuseTicket>);

#endif  // ORTEAF_ENABLE_MPS

/**
 * @file mps_reuse_token_test.mm
 * @brief Tests for MpsReuseToken bundling MpsReuseTicket.
 */

#import <Metal/Metal.h>

#include <gtest/gtest.h>
#include <type_traits>

#include "orteaf/internal/backend/mps/mps_reuse_ticket.h"
#include "orteaf/internal/backend/mps/mps_reuse_token.h"
#include "orteaf/internal/backend/mps/wrapper/mps_command_buffer.h"
#include "orteaf/internal/backend/mps/wrapper/mps_command_queue.h"
#include "orteaf/internal/backend/mps/wrapper/mps_device.h"

namespace mps_backend = orteaf::internal::backend::mps;
namespace base = orteaf::internal::base;

class MpsReuseTokenTest : public ::testing::Test {
protected:
    void SetUp() override {
#if ORTEAF_ENABLE_MPS
        device_ = mps_backend::getDevice();
        if (device_ == nullptr) {
            GTEST_SKIP() << "No Metal devices available";
        }
        queue_ = mps_backend::createCommandQueue(device_);
        if (queue_ == nullptr) {
            GTEST_SKIP() << "Failed to create command queue";
        }
        command_buffer_a_ = mps_backend::createCommandBuffer(queue_);
        command_buffer_b_ = mps_backend::createCommandBuffer(queue_);
        if (command_buffer_a_ == nullptr || command_buffer_b_ == nullptr) {
            GTEST_SKIP() << "Failed to create command buffers";
        }
#else
        GTEST_SKIP() << "MPS not enabled";
#endif
    }

    void TearDown() override {
#if ORTEAF_ENABLE_MPS
        if (command_buffer_a_ != nullptr) {
            mps_backend::destroyCommandBuffer(command_buffer_a_);
            command_buffer_a_ = nullptr;
        }
        if (command_buffer_b_ != nullptr) {
            mps_backend::destroyCommandBuffer(command_buffer_b_);
            command_buffer_b_ = nullptr;
        }
        if (queue_ != nullptr) {
            mps_backend::destroyCommandQueue(queue_);
            queue_ = nullptr;
        }
        if (device_ != nullptr) {
            mps_backend::deviceRelease(device_);
            device_ = nullptr;
        }
#endif
    }

#if ORTEAF_ENABLE_MPS
    mps_backend::MPSDevice_t device_{nullptr};
    mps_backend::MPSCommandQueue_t queue_{nullptr};
    mps_backend::MPSCommandBuffer_t command_buffer_a_{nullptr};
    mps_backend::MPSCommandBuffer_t command_buffer_b_{nullptr};
    base::CommandQueueId queue_id_{base::CommandQueueId{13}};
#endif
};

#if ORTEAF_ENABLE_MPS

TEST_F(MpsReuseTokenTest, DefaultConstructedIsEmpty) {
    mps_backend::MpsReuseToken token;
    EXPECT_TRUE(token.empty());
    EXPECT_EQ(token.size(), 0u);
}

TEST_F(MpsReuseTokenTest, AddTicketsStoresAndOrders) {
    mps_backend::MpsReuseToken token;

    token.addTicket(mps_backend::MpsReuseTicket(queue_id_, command_buffer_a_));
    token.addTicket(mps_backend::MpsReuseTicket(queue_id_, command_buffer_b_));

    ASSERT_EQ(token.size(), 2u);
    EXPECT_EQ(token[0].commandQueueId(), queue_id_);
    EXPECT_EQ(token[0].commandBuffer(), command_buffer_a_);
    EXPECT_EQ(token[1].commandQueueId(), queue_id_);
    EXPECT_EQ(token[1].commandBuffer(), command_buffer_b_);
}

TEST_F(MpsReuseTokenTest, MoveTransfersOwnership) {
    mps_backend::MpsReuseToken token;
    token.addTicket(mps_backend::MpsReuseTicket(queue_id_, command_buffer_a_));

    mps_backend::MpsReuseToken moved(std::move(token));
    EXPECT_EQ(moved.size(), 1u);
    EXPECT_FALSE(moved.empty());

    EXPECT_TRUE(token.empty());
    EXPECT_EQ(token.size(), 0u);
}

TEST_F(MpsReuseTokenTest, ClearRemovesAllTickets) {
    mps_backend::MpsReuseToken token;
    token.addTicket(mps_backend::MpsReuseTicket(queue_id_, command_buffer_a_));

    token.clear();
    EXPECT_TRUE(token.empty());
    EXPECT_EQ(token.size(), 0u);
}

static_assert(!std::is_copy_constructible_v<mps_backend::MpsReuseToken>);
static_assert(!std::is_copy_assignable_v<mps_backend::MpsReuseToken>);

#endif  // ORTEAF_ENABLE_MPS

/**
 * @file mps_fence_ticket_test.mm
 * @brief Tests for MpsFenceTicket / MpsReuseTicket.
 */

#import <Metal/Metal.h>

#include <gtest/gtest.h>
#include <type_traits>

#include "orteaf/internal/backend/mps/mps_fence_ticket.h"
#include "orteaf/internal/backend/mps/wrapper/mps_command_buffer.h"
#include "orteaf/internal/backend/mps/wrapper/mps_command_queue.h"
#include "orteaf/internal/backend/mps/wrapper/mps_device.h"
#include "orteaf/internal/backend/mps/wrapper/mps_fence.h"
#include "orteaf/internal/backend/mps/mps_slow_ops.h"

namespace mps_backend = orteaf::internal::backend::mps;
namespace mps_rt = orteaf::internal::runtime::mps;
namespace base = orteaf::internal::base;

class MpsFenceTicketTest : public ::testing::Test {
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
        fence_pool_.initialize(device_, &ops_, 1);
#else
        GTEST_SKIP() << "MPS not enabled";
#endif
    }

    void TearDown() override {
#if ORTEAF_ENABLE_MPS
        fence_pool_.shutdown();
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
    mps_rt::MpsFencePool fence_pool_{};
    ::orteaf::internal::runtime::backend_ops::mps::MpsSlowOpsImpl ops_{};
    base::CommandQueueId queue_id_{base::CommandQueueId{7}};
#endif
};

#if ORTEAF_ENABLE_MPS

TEST_F(MpsFenceTicketTest, DefaultConstructedIsInvalid) {
    mps_backend::MpsFenceTicket ticket;
    EXPECT_FALSE(ticket.valid());
    EXPECT_FALSE(ticket.hasFence());
    EXPECT_EQ(ticket.commandQueueId(), base::CommandQueueId{});
    EXPECT_EQ(ticket.commandBuffer(), nullptr);
}

TEST_F(MpsFenceTicketTest, ValueConstructorStoresMembers) {
    auto handle = fence_pool_.acquireFence();
    mps_backend::MpsFenceTicket ticket(queue_id_, command_buffer_, std::move(handle));

    EXPECT_TRUE(ticket.valid());
    EXPECT_TRUE(ticket.hasFence());
    EXPECT_EQ(ticket.commandQueueId(), queue_id_);
    EXPECT_EQ(ticket.commandBuffer(), command_buffer_);

    id<MTLFence> objc_fence = (__bridge id<MTLFence>)ticket.fenceHandle().get();
    EXPECT_NE(objc_fence, nil);
    id<MTLCommandBuffer> objc_cb = (__bridge id<MTLCommandBuffer>)ticket.commandBuffer();
    EXPECT_NE(objc_cb, nil);
}

TEST_F(MpsFenceTicketTest, SettersUpdateMembers) {
    mps_backend::MpsFenceTicket ticket;
    auto handle = fence_pool_.acquireFence();

    ticket.setCommandQueueId(queue_id_)
          .setCommandBuffer(command_buffer_)
          .setFenceHandle(std::move(handle));

    EXPECT_TRUE(ticket.valid());
    EXPECT_TRUE(ticket.hasFence());
    EXPECT_EQ(ticket.commandQueueId(), queue_id_);
    EXPECT_EQ(ticket.commandBuffer(), command_buffer_);
    EXPECT_NE(ticket.fenceHandle().get(), nullptr);
}

TEST_F(MpsFenceTicketTest, MoveTransfersOwnership) {
    auto handle = fence_pool_.acquireFence();
    mps_backend::MpsFenceTicket ticket(queue_id_, command_buffer_, std::move(handle));

    mps_backend::MpsFenceTicket moved(std::move(ticket));

    EXPECT_TRUE(moved.valid());
    EXPECT_TRUE(moved.hasFence());
    EXPECT_EQ(moved.commandQueueId(), queue_id_);
    EXPECT_EQ(moved.commandBuffer(), command_buffer_);
    EXPECT_NE(moved.fenceHandle().get(), nullptr);

    EXPECT_FALSE(ticket.valid());
    EXPECT_FALSE(ticket.hasFence());
    EXPECT_EQ(ticket.commandBuffer(), nullptr);
}

TEST_F(MpsFenceTicketTest, ResetClearsState) {
    auto handle = fence_pool_.acquireFence();
    mps_backend::MpsFenceTicket ticket(queue_id_, command_buffer_, std::move(handle));

    ticket.reset();

    EXPECT_FALSE(ticket.valid());
    EXPECT_FALSE(ticket.hasFence());
    EXPECT_EQ(ticket.commandBuffer(), nullptr);
    EXPECT_EQ(ticket.commandQueueId(), base::CommandQueueId{});
}

static_assert(!std::is_copy_constructible_v<mps_backend::MpsFenceTicket>);
static_assert(!std::is_copy_assignable_v<mps_backend::MpsFenceTicket>);

#endif  // ORTEAF_ENABLE_MPS

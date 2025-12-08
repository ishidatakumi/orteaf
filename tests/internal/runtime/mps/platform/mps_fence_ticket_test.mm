/**
 * @file mps_fence_ticket_test.mm
 * @brief Tests for MpsFenceTicket / MpsReuseTicket.
 */

#import <Metal/Metal.h>

#include <gtest/gtest.h>
#include <type_traits>

#include "orteaf/internal/runtime/mps/platform/mps_slow_ops.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_command_buffer.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_command_queue.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_device.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_fence.h"
#include "orteaf/internal/runtime/mps/resource/mps_fence_ticket.h"

namespace mps_wrapper = orteaf::internal::runtime::mps::platform::wrapper;
namespace mps_res = orteaf::internal::runtime::mps::resource;
namespace mps_rt = orteaf::internal::runtime::mps;
namespace base = orteaf::internal::base;

class MpsFenceTicketTest : public ::testing::Test {
protected:
  void SetUp() override {
#if ORTEAF_ENABLE_MPS
    device_ = mps_wrapper::getDevice();
    if (device_ == nullptr) {
      GTEST_SKIP() << "No Metal devices available";
    }
    command_queue_ = mps_wrapper::createCommandQueue(device_);
    if (command_queue_ == nullptr) {
      GTEST_SKIP() << "Failed to create command queue";
    }
    command_buffer_ = mps_wrapper::createCommandBuffer(command_queue_);
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
      mps_wrapper::destroyCommandBuffer(command_buffer_);
      command_buffer_ = nullptr;
    }
    if (command_queue_ != nullptr) {
      mps_wrapper::destroyCommandQueue(command_queue_);
      command_queue_ = nullptr;
    }
    if (device_ != nullptr) {
      mps_wrapper::deviceRelease(device_);
      device_ = nullptr;
    }
#endif
  }

#if ORTEAF_ENABLE_MPS
  mps_wrapper::MPSDevice_t device_{nullptr};
  mps_wrapper::MPSCommandQueue_t command_queue_{nullptr};
  mps_wrapper::MPSCommandBuffer_t command_buffer_{nullptr};
  mps_rt::manager::MpsFencePool fence_pool_{};
  ::orteaf::internal::runtime::mps::platform::MpsSlowOpsImpl ops_{};
  base::CommandQueueHandle queue_id_{base::CommandQueueHandle{7}};
#endif
};

#if ORTEAF_ENABLE_MPS

TEST_F(MpsFenceTicketTest, DefaultConstructedIsInvalid) {
  mps_res::MpsFenceTicket ticket;
  EXPECT_FALSE(ticket.valid());
  EXPECT_FALSE(ticket.hasFence());
  EXPECT_EQ(ticket.commandQueueHandle(), base::CommandQueueHandle{});
  EXPECT_EQ(ticket.commandBuffer(), nullptr);
}

TEST_F(MpsFenceTicketTest, ValueConstructorStoresMembers) {
  auto handle = fence_pool_.acquireFence();
  mps_res::MpsFenceTicket ticket(queue_id_, command_buffer_, std::move(handle));

  EXPECT_TRUE(ticket.valid());
  EXPECT_TRUE(ticket.hasFence());
  EXPECT_EQ(ticket.commandQueueHandle(), queue_id_);
  EXPECT_EQ(ticket.commandBuffer(), command_buffer_);
  id<MTLFence> objc_fence =
      (__bridge id<MTLFence>)(ticket.fenceHandle().pointer());
  EXPECT_NE(objc_fence, nil);
  id<MTLCommandBuffer> objc_cb =
      (__bridge id<MTLCommandBuffer>)ticket.commandBuffer();
  EXPECT_NE(objc_cb, nil);
}

TEST_F(MpsFenceTicketTest, SettersUpdateMembers) {
  mps_res::MpsFenceTicket ticket;
  auto handle = fence_pool_.acquireFence();

  ticket.setCommandQueueHandle(queue_id_)
      .setCommandBuffer(command_buffer_)
      .setFenceHandle(std::move(handle));

  EXPECT_TRUE(ticket.valid());
  EXPECT_TRUE(ticket.hasFence());
  EXPECT_EQ(ticket.commandQueueHandle(), queue_id_);
  EXPECT_EQ(ticket.commandBuffer(), command_buffer_);
  EXPECT_TRUE(ticket.fenceHandle());
}

TEST_F(MpsFenceTicketTest, MoveTransfersOwnership) {
  auto handle = fence_pool_.acquireFence();
  mps_res::MpsFenceTicket ticket(queue_id_, command_buffer_, std::move(handle));

  mps_res::MpsFenceTicket moved(std::move(ticket));

  EXPECT_TRUE(moved.valid());
  EXPECT_TRUE(moved.hasFence());
  EXPECT_EQ(moved.commandQueueHandle(), queue_id_);
  EXPECT_EQ(moved.commandBuffer(), command_buffer_);
  EXPECT_TRUE(moved.fenceHandle());

  EXPECT_FALSE(ticket.valid());
  EXPECT_FALSE(ticket.hasFence());
  EXPECT_EQ(ticket.commandBuffer(), nullptr);
}

TEST_F(MpsFenceTicketTest, ResetClearsState) {
  auto handle = fence_pool_.acquireFence();
  mps_res::MpsFenceTicket ticket(queue_id_, command_buffer_, std::move(handle));

  ticket.reset();

  EXPECT_FALSE(ticket.valid());
  EXPECT_FALSE(ticket.hasFence());
  EXPECT_EQ(ticket.commandBuffer(), nullptr);
  EXPECT_EQ(ticket.commandQueueHandle(), base::CommandQueueHandle{});
}

static_assert(!std::is_copy_constructible_v<mps_res::MpsFenceTicket>);
static_assert(!std::is_copy_assignable_v<mps_res::MpsFenceTicket>);

#endif // ORTEAF_ENABLE_MPS

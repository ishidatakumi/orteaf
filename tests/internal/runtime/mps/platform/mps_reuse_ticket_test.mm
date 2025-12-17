/**
 * @file mps_reuse_ticket_test.mm
 * @brief Tests for MpsReuseTicket.
 */

#import <Metal/Metal.h>

#include <gtest/gtest.h>
#include <type_traits>

#include "orteaf/internal/runtime/mps/platform/wrapper/mps_command_buffer.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_command_queue.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_device.h"
#include "orteaf/internal/runtime/mps/resource/mps_reuse_ticket.h"

namespace mps_wrapper = orteaf::internal::runtime::mps::platform::wrapper;
namespace mps_res = orteaf::internal::runtime::mps::resource;
namespace base = orteaf::internal::base;

class MpsReuseTicketTest : public ::testing::Test {
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
#else
    GTEST_SKIP() << "MPS not enabled";
#endif
  }

  void TearDown() override {
#if ORTEAF_ENABLE_MPS
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
  mps_wrapper::MpsDevice_t device_{nullptr};
  mps_wrapper::MpsCommandQueue_t command_queue_{nullptr};
  mps_wrapper::MpsCommandBuffer_t command_buffer_{nullptr};
  base::CommandQueueHandle queue_id_{base::CommandQueueHandle{9}};
#endif
};

#if ORTEAF_ENABLE_MPS

TEST_F(MpsReuseTicketTest, DefaultConstructedIsInvalid) {
  mps_res::MpsReuseTicket ticket;
  EXPECT_FALSE(ticket.valid());
  EXPECT_EQ(ticket.commandQueueHandle(), base::CommandQueueHandle{});
  EXPECT_EQ(ticket.commandBuffer(), nullptr);
}

TEST_F(MpsReuseTicketTest, StoresMembersFromConstructor) {
  mps_res::MpsReuseTicket ticket(queue_id_, command_buffer_);
  EXPECT_TRUE(ticket.valid());
  EXPECT_EQ(ticket.commandQueueHandle(), queue_id_);
  EXPECT_EQ(ticket.commandBuffer(), command_buffer_);
}

TEST_F(MpsReuseTicketTest, SettersAndMoveTransferState) {
  mps_res::MpsReuseTicket ticket;
  ticket.setCommandQueueHandle(queue_id_).setCommandBuffer(command_buffer_);

  EXPECT_TRUE(ticket.valid());
  EXPECT_EQ(ticket.commandQueueHandle(), queue_id_);
  EXPECT_EQ(ticket.commandBuffer(), command_buffer_);

  mps_res::MpsReuseTicket moved(std::move(ticket));
  EXPECT_TRUE(moved.valid());
  EXPECT_EQ(moved.commandQueueHandle(), queue_id_);
  EXPECT_EQ(moved.commandBuffer(), command_buffer_);

  EXPECT_FALSE(ticket.valid());
  EXPECT_EQ(ticket.commandBuffer(), nullptr);
}

static_assert(!std::is_copy_constructible_v<mps_res::MpsReuseTicket>);
static_assert(!std::is_copy_assignable_v<mps_res::MpsReuseTicket>);

#endif // ORTEAF_ENABLE_MPS

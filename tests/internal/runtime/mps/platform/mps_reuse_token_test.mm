/**
 * @file mps_reuse_token_test.mm
 * @brief Tests for MpsReuseToken bundling MpsReuseTicket.
 */

#import <Metal/Metal.h>

#include <gtest/gtest.h>
#include <type_traits>

#include "orteaf/internal/runtime/mps/platform/wrapper/mps_command_buffer.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_command_queue.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_device.h"
#include "orteaf/internal/runtime/mps/resource/mps_reuse_ticket.h"
#include "orteaf/internal/runtime/mps/resource/mps_reuse_token.h"

namespace mps_wrapper = orteaf::internal::runtime::mps::platform::wrapper;
namespace mps_res = orteaf::internal::runtime::mps::resource;
namespace base = orteaf::internal::base;

class MpsReuseTokenTest : public ::testing::Test {
protected:
  void SetUp() override {
#if ORTEAF_ENABLE_MPS
    device_ = mps_wrapper::getDevice();
    if (device_ == nullptr) {
      GTEST_SKIP() << "No Metal devices available";
    }
    queue_ = mps_wrapper::createCommandQueue(device_);
    if (queue_ == nullptr) {
      GTEST_SKIP() << "Failed to create command queue";
    }
    command_buffer_a_ = mps_wrapper::createCommandBuffer(queue_);
    command_buffer_b_ = mps_wrapper::createCommandBuffer(queue_);
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
      mps_wrapper::destroyCommandBuffer(command_buffer_a_);
      command_buffer_a_ = nullptr;
    }
    if (command_buffer_b_ != nullptr) {
      mps_wrapper::destroyCommandBuffer(command_buffer_b_);
      command_buffer_b_ = nullptr;
    }
    if (queue_ != nullptr) {
      mps_wrapper::destroyCommandQueue(queue_);
      queue_ = nullptr;
    }
    if (device_ != nullptr) {
      mps_wrapper::deviceRelease(device_);
      device_ = nullptr;
    }
#endif
  }

#if ORTEAF_ENABLE_MPS
  mps_wrapper::MpsDevice_t device_{nullptr};
  mps_wrapper::MpsCommandQueue_t queue_{nullptr};
  mps_wrapper::MpsCommandBuffer_t command_buffer_a_{nullptr};
  mps_wrapper::MpsCommandBuffer_t command_buffer_b_{nullptr};
  base::CommandQueueHandle queue_id_{base::CommandQueueHandle{13}};
#endif
};

#if ORTEAF_ENABLE_MPS

TEST_F(MpsReuseTokenTest, DefaultConstructedIsEmpty) {
  mps_res::MpsReuseToken token;
  EXPECT_TRUE(token.empty());
  EXPECT_EQ(token.size(), 0u);
}

TEST_F(MpsReuseTokenTest, AddTicketsStoresAndOrders) {
  mps_res::MpsReuseToken token;

  token.addTicket(mps_res::MpsReuseTicket(queue_id_, command_buffer_a_));
  token.addTicket(mps_res::MpsReuseTicket(queue_id_, command_buffer_b_));

  ASSERT_EQ(token.size(), 2u);
  EXPECT_EQ(token[0].commandQueueHandle(), queue_id_);
  EXPECT_EQ(token[0].commandBuffer(), command_buffer_a_);
  EXPECT_EQ(token[1].commandQueueHandle(), queue_id_);
  EXPECT_EQ(token[1].commandBuffer(), command_buffer_b_);
}

TEST_F(MpsReuseTokenTest, MoveTransfersOwnership) {
  mps_res::MpsReuseToken token;
  token.addTicket(mps_res::MpsReuseTicket(queue_id_, command_buffer_a_));

  mps_res::MpsReuseToken moved(std::move(token));
  EXPECT_EQ(moved.size(), 1u);
  EXPECT_FALSE(moved.empty());

  EXPECT_TRUE(token.empty());
  EXPECT_EQ(token.size(), 0u);
}

TEST_F(MpsReuseTokenTest, ClearRemovesAllTickets) {
  mps_res::MpsReuseToken token;
  token.addTicket(mps_res::MpsReuseTicket(queue_id_, command_buffer_a_));

  token.clear();
  EXPECT_TRUE(token.empty());
  EXPECT_EQ(token.size(), 0u);
}

static_assert(!std::is_copy_constructible_v<mps_res::MpsReuseToken>);
static_assert(!std::is_copy_assignable_v<mps_res::MpsReuseToken>);

#endif // ORTEAF_ENABLE_MPS

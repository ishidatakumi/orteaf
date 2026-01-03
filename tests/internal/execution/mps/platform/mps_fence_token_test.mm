/**
 * @file mps_fence_token_test.mm
 * @brief Tests for MpsFenceToken storing fence leases.
 */

#import <Metal/Metal.h>

#include <gtest/gtest.h>
#include <type_traits>

#include "orteaf/internal/execution/mps/platform/mps_slow_ops.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_command_buffer.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_command_queue.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_device.h"
#include "orteaf/internal/execution/mps/resource/mps_fence_token.h"

namespace mps_wrapper = orteaf::internal::execution::mps::platform::wrapper;
namespace mps_res = orteaf::internal::execution::mps::resource;
namespace mps_rt = orteaf::internal::execution::mps;
namespace base = orteaf::internal::base;

class MpsFenceTokenTest : public ::testing::Test {
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
    fence_pool_.configure(mps_rt::manager::MpsFenceManager::Config{
        device_, &ops_, 2, 2, 2, 2, 1, 1});
#else
    GTEST_SKIP() << "MPS not enabled";
#endif
  }

  void TearDown() override {
#if ORTEAF_ENABLE_MPS
    fence_pool_.shutdown();
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
  mps_rt::manager::MpsFenceManager fence_pool_{};
  ::orteaf::internal::execution::mps::platform::MpsSlowOpsImpl ops_{};
  base::CommandQueueHandle queue_id_{base::CommandQueueHandle{11}};
  base::CommandQueueHandle queue_id_other_{base::CommandQueueHandle{42}};
#endif
};

#if ORTEAF_ENABLE_MPS

TEST_F(MpsFenceTokenTest, DefaultConstructedIsEmpty) {
  mps_res::MpsFenceToken token;
  EXPECT_TRUE(token.empty());
  EXPECT_EQ(token.size(), 0u);
}

TEST_F(MpsFenceTokenTest, AddOrReplaceLeaseAddsNew) {
  mps_res::MpsFenceToken token;
  auto handle_a = fence_pool_.acquire();
  auto handle_b = fence_pool_.acquire();

  auto *payload_a = handle_a.payloadPtr();
  auto *payload_b = handle_b.payloadPtr();
  ASSERT_NE(payload_a, nullptr);
  ASSERT_NE(payload_b, nullptr);
  EXPECT_TRUE(payload_a->setCommandQueueHandle(queue_id_));
  EXPECT_TRUE(payload_a->setCommandBuffer(command_buffer_a_));
  EXPECT_TRUE(payload_b->setCommandQueueHandle(queue_id_other_));
  EXPECT_TRUE(payload_b->setCommandBuffer(command_buffer_b_));

  token.addOrReplaceLease(std::move(handle_a));
  token.addOrReplaceLease(std::move(handle_b));

  ASSERT_EQ(token.size(), 2u);
  auto *lease_a_payload = token[0].payloadPtr();
  ASSERT_NE(lease_a_payload, nullptr);
  EXPECT_EQ(lease_a_payload->commandQueueHandle(), queue_id_);
  EXPECT_EQ(lease_a_payload->commandBuffer(), command_buffer_a_);
  EXPECT_TRUE(lease_a_payload->hasFence());

  auto *lease_b_payload = token[1].payloadPtr();
  ASSERT_NE(lease_b_payload, nullptr);
  EXPECT_EQ(lease_b_payload->commandQueueHandle(), queue_id_other_);
  EXPECT_EQ(lease_b_payload->commandBuffer(), command_buffer_b_);
  EXPECT_TRUE(lease_b_payload->hasFence());
}

TEST_F(MpsFenceTokenTest, AddOrReplaceLeaseReplacesExisting) {
  mps_res::MpsFenceToken token;
  auto handle_a = fence_pool_.acquire();
  auto handle_b = fence_pool_.acquire();

  auto *payload_a = handle_a.payloadPtr();
  auto *payload_b = handle_b.payloadPtr();
  ASSERT_NE(payload_a, nullptr);
  ASSERT_NE(payload_b, nullptr);
  EXPECT_TRUE(payload_a->setCommandQueueHandle(queue_id_));
  EXPECT_TRUE(payload_a->setCommandBuffer(command_buffer_a_));
  EXPECT_TRUE(payload_b->setCommandQueueHandle(queue_id_));
  EXPECT_TRUE(payload_b->setCommandBuffer(command_buffer_b_));

  token.addOrReplaceLease(std::move(handle_a));
  ASSERT_EQ(token.size(), 1u);
  EXPECT_EQ(token[0].payloadPtr()->commandBuffer(), command_buffer_a_);

  token.addOrReplaceLease(std::move(handle_b));
  ASSERT_EQ(token.size(), 1u);
  EXPECT_EQ(token[0].payloadPtr()->commandBuffer(), command_buffer_b_);
}

TEST_F(MpsFenceTokenTest, MoveTransfersOwnership) {
  mps_res::MpsFenceToken token;
  auto handle = fence_pool_.acquire();
  auto *payload = handle.payloadPtr();
  ASSERT_NE(payload, nullptr);
  EXPECT_TRUE(payload->setCommandQueueHandle(queue_id_));
  EXPECT_TRUE(payload->setCommandBuffer(command_buffer_a_));
  token.addOrReplaceLease(std::move(handle));

  mps_res::MpsFenceToken moved(std::move(token));
  EXPECT_EQ(moved.size(), 1u);
  EXPECT_FALSE(moved.empty());

  EXPECT_TRUE(token.empty());
  EXPECT_EQ(token.size(), 0u);
}

TEST_F(MpsFenceTokenTest, ClearRemovesAllLeases) {
  mps_res::MpsFenceToken token;
  auto handle = fence_pool_.acquire();
  auto *payload = handle.payloadPtr();
  ASSERT_NE(payload, nullptr);
  EXPECT_TRUE(payload->setCommandQueueHandle(queue_id_));
  EXPECT_TRUE(payload->setCommandBuffer(command_buffer_a_));
  token.addOrReplaceLease(std::move(handle));

  token.clear();
  EXPECT_TRUE(token.empty());
  EXPECT_EQ(token.size(), 0u);
}

static_assert(!std::is_copy_constructible_v<mps_res::MpsFenceToken>);
static_assert(!std::is_copy_assignable_v<mps_res::MpsFenceToken>);

#endif // ORTEAF_ENABLE_MPS

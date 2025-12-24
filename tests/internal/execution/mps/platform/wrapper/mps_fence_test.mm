/**
 * @file mps_fence_test.mm
 * @brief Tests for MPS/Metal fence helpers.
 */

#import <Metal/Metal.h>

#include "orteaf/internal/execution/mps/platform/wrapper/mps_command_buffer.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_command_queue.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_compute_command_encoder.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_device.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_fence.h"

#include <gtest/gtest.h>

#include "tests/internal/testing/error_assert.h"

namespace mps = orteaf::internal::execution::mps::platform::wrapper;

class MpsFenceTest : public ::testing::Test {
protected:
  void SetUp() override {
    device_ = mps::getDevice();
    if (device_ == nullptr) {
      GTEST_SKIP() << "No Metal devices available";
    }
    queue_ = mps::createCommandQueue(device_);
    if (queue_ == nullptr) {
      GTEST_SKIP() << "Failed to create command queue";
    }
  }

  void TearDown() override {
    if (queue_ != nullptr) {
      mps::destroyCommandQueue(queue_);
    }
    if (device_ != nullptr) {
      mps::deviceRelease(device_);
    }
  }

  mps::MpsDevice_t device_{nullptr};
  mps::MpsCommandQueue_t queue_{nullptr};
};

TEST_F(MpsFenceTest, CreateFenceSucceeds) {
  mps::MpsFence_t fence = mps::createFence(device_);
  ASSERT_NE(fence, nullptr);

  id<MTLFence> objc_fence = (__bridge id<MTLFence>)fence;
  EXPECT_NE(objc_fence, nil);

  mps::destroyFence(fence);
}

TEST_F(MpsFenceTest, DestroyFenceIgnoresNullptr) {
  EXPECT_NO_THROW(mps::destroyFence(nullptr));
}

TEST_F(MpsFenceTest, DestroyFenceReleasesHandle) {
  mps::MpsFence_t fence = mps::createFence(device_);
  ASSERT_NE(fence, nullptr);
  EXPECT_NO_THROW(mps::destroyFence(fence));
}

TEST_F(MpsFenceTest, UpdateAndWaitFenceOnComputeEncoder) {
  mps::MpsFence_t fence = mps::createFence(device_);
  ASSERT_NE(fence, nullptr);

  auto command_buffer = mps::createCommandBuffer(queue_);
  ASSERT_NE(command_buffer, nullptr);
  auto encoder = mps::createComputeCommandEncoder(command_buffer);
  ASSERT_NE(encoder, nullptr);

  EXPECT_NO_THROW(mps::updateFence(encoder, fence));
  EXPECT_NO_THROW(mps::waitForFence(encoder, fence));

  mps::endEncoding(encoder);
  mps::commit(command_buffer);
  mps::waitUntilCompleted(command_buffer);
  mps::destroyCommandBuffer(command_buffer);

  mps::destroyFence(fence);
}

TEST_F(MpsFenceTest, UpdateFenceNullEncoderThrows) {
  mps::MpsFence_t fence = mps::createFence(device_);
  ASSERT_NE(fence, nullptr);
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [&] { mps::updateFence(nullptr, fence); });
  mps::destroyFence(fence);
}

TEST_F(MpsFenceTest, UpdateFenceNullFenceThrows) {
  auto command_buffer = mps::createCommandBuffer(queue_);
  ASSERT_NE(command_buffer, nullptr);
  auto encoder = mps::createComputeCommandEncoder(command_buffer);
  ASSERT_NE(encoder, nullptr);
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [&] { mps::updateFence(encoder, nullptr); });
  mps::endEncoding(encoder);
  mps::destroyCommandBuffer(command_buffer);
}

TEST_F(MpsFenceTest, WaitFenceNullEncoderThrows) {
  mps::MpsFence_t fence = mps::createFence(device_);
  ASSERT_NE(fence, nullptr);
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [&] { mps::waitForFence(nullptr, fence); });
  mps::destroyFence(fence);
}

TEST_F(MpsFenceTest, WaitFenceNullFenceThrows) {
  auto command_buffer = mps::createCommandBuffer(queue_);
  ASSERT_NE(command_buffer, nullptr);
  auto encoder = mps::createComputeCommandEncoder(command_buffer);
  ASSERT_NE(encoder, nullptr);
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [&] { mps::waitForFence(encoder, nullptr); });
  mps::endEncoding(encoder);
  mps::destroyCommandBuffer(command_buffer);
}

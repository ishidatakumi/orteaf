/**
 * @file mps_command_queue_buffer_test.mm
 * @brief Tests for MPS/Metal command queue and command buffer operations.
 */

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include "orteaf/internal/execution/mps/platform/wrapper/mps_command_buffer.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_command_queue.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_device.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_event.h"

#include "tests/internal/testing/error_assert.h"

#include <gtest/gtest.h>

namespace mps = orteaf::internal::execution::mps::platform::wrapper;

/**
 * @brief Test fixture for MPS command queue and buffer tests.
 */
class MpsCommandQueueBufferTest : public ::testing::Test {
protected:
  void SetUp() override {
    device_ = mps::getDevice();
    if (device_ == nullptr) {
      GTEST_SKIP() << "No Metal devices available";
    }
  }

  void TearDown() override {
    if (device_ != nullptr) {
      mps::deviceRelease(device_);
    }
  }

  mps::MpsDevice_t device_ = nullptr;
};

/**
 * @brief Test that command queue can be created.
 */
TEST_F(MpsCommandQueueBufferTest, CreateCommandQueueSucceeds) {
  mps::MpsCommandQueue_t queue = mps::createCommandQueue(device_);
  EXPECT_NE(queue, nullptr);

  // Verify it's a valid MTLCommandQueue
  id<MTLCommandQueue> objc_queue = (__bridge id<MTLCommandQueue>)queue;
  EXPECT_NE(objc_queue, nil);

  mps::destroyCommandQueue(queue);
}

TEST_F(MpsCommandQueueBufferTest, CreateCommandQueueNullDeviceThrows) {
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [] { mps::createCommandQueue(nullptr); });
}

/**
 * @brief Test that command queue can be destroyed.
 */
TEST_F(MpsCommandQueueBufferTest, DestroyCommandQueueSucceeds) {
  mps::MpsCommandQueue_t queue = mps::createCommandQueue(device_);
  EXPECT_NE(queue, nullptr);

  EXPECT_NO_THROW(mps::destroyCommandQueue(queue));
}

/**
 * @brief Test that destroy_command_queue with nullptr is ignored.
 */
TEST_F(MpsCommandQueueBufferTest, DestroyCommandQueueNullptrIsIgnored) {
  EXPECT_NO_THROW(mps::destroyCommandQueue(nullptr));
}

/**
 * @brief Test that multiple command queues can be created.
 */
TEST_F(MpsCommandQueueBufferTest, CreateMultipleCommandQueues) {
  mps::MpsCommandQueue_t queue1 = mps::createCommandQueue(device_);
  mps::MpsCommandQueue_t queue2 = mps::createCommandQueue(device_);

  EXPECT_NE(queue1, nullptr);
  EXPECT_NE(queue2, nullptr);
  EXPECT_NE(queue1, queue2);

  mps::destroyCommandQueue(queue1);
  mps::destroyCommandQueue(queue2);
}

/**
 * @brief Test that command buffer can be created from queue.
 */
TEST_F(MpsCommandQueueBufferTest, CreateCommandBufferSucceeds) {
  mps::MpsCommandQueue_t queue = mps::createCommandQueue(device_);
  ASSERT_NE(queue, nullptr);

  mps::MpsCommandBuffer_t buffer = mps::createCommandBuffer(queue);
  EXPECT_NE(buffer, nullptr);

  // Verify it's a valid MTLCommandBuffer
  id<MTLCommandBuffer> objc_buffer = (__bridge id<MTLCommandBuffer>)buffer;
  EXPECT_NE(objc_buffer, nil);

  mps::destroyCommandBuffer(buffer);
  mps::destroyCommandQueue(queue);
}

/**
 * @brief Test that command buffer can be destroyed.
 */
TEST_F(MpsCommandQueueBufferTest, DestroyCommandBufferSucceeds) {
  mps::MpsCommandQueue_t queue = mps::createCommandQueue(device_);
  ASSERT_NE(queue, nullptr);

  mps::MpsCommandBuffer_t buffer = mps::createCommandBuffer(queue);
  ASSERT_NE(buffer, nullptr);

  EXPECT_NO_THROW(mps::destroyCommandBuffer(buffer));
  mps::destroyCommandQueue(queue);
}

/**
 * @brief Test that destroy_command_buffer with nullptr is ignored.
 */
TEST_F(MpsCommandQueueBufferTest, DestroyCommandBufferNullptrIsIgnored) {
  EXPECT_NO_THROW(mps::destroyCommandBuffer(nullptr));
}

TEST_F(MpsCommandQueueBufferTest, CreateCommandBufferNullQueueThrows) {
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [] { mps::createCommandBuffer(nullptr); });
}

/**
 * @brief Test that multiple command buffers can be created.
 */
TEST_F(MpsCommandQueueBufferTest, CreateMultipleCommandBuffers) {
  mps::MpsCommandQueue_t queue = mps::createCommandQueue(device_);
  ASSERT_NE(queue, nullptr);

  mps::MpsCommandBuffer_t buffer1 = mps::createCommandBuffer(queue);
  mps::MpsCommandBuffer_t buffer2 = mps::createCommandBuffer(queue);

  EXPECT_NE(buffer1, nullptr);
  EXPECT_NE(buffer2, nullptr);
  EXPECT_NE(buffer1, buffer2);

  mps::destroyCommandBuffer(buffer1);
  mps::destroyCommandBuffer(buffer2);
  mps::destroyCommandQueue(queue);
}

/**
 * @brief Test that command buffer can be committed.
 */
TEST_F(MpsCommandQueueBufferTest, CommitCommandBufferSucceeds) {
  mps::MpsCommandQueue_t queue = mps::createCommandQueue(device_);
  ASSERT_NE(queue, nullptr);

  mps::MpsCommandBuffer_t buffer = mps::createCommandBuffer(queue);
  ASSERT_NE(buffer, nullptr);

  EXPECT_NO_THROW(mps::commit(buffer));

  mps::destroyCommandBuffer(buffer);
  mps::destroyCommandQueue(queue);
}

TEST_F(MpsCommandQueueBufferTest, CommitNullCommandBufferThrows) {
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [] { mps::commit(nullptr); });
}

/**
 * @brief Test that command buffer can be waited for completion.
 */
TEST_F(MpsCommandQueueBufferTest, WaitUntilCompletedSucceeds) {
  mps::MpsCommandQueue_t queue = mps::createCommandQueue(device_);
  ASSERT_NE(queue, nullptr);

  mps::MpsCommandBuffer_t buffer = mps::createCommandBuffer(queue);
  ASSERT_NE(buffer, nullptr);

  mps::commit(buffer);
  EXPECT_NO_THROW(mps::waitUntilCompleted(buffer));

  mps::destroyCommandBuffer(buffer);
  mps::destroyCommandQueue(queue);
}

TEST_F(MpsCommandQueueBufferTest, WaitUntilCompletedNullCommandBufferThrows) {
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [] { mps::waitUntilCompleted(nullptr); });
}

/**
 * @brief Test complete command buffer lifecycle.
 */
TEST_F(MpsCommandQueueBufferTest, CommandBufferLifecycle) {
  mps::MpsCommandQueue_t queue = mps::createCommandQueue(device_);
  ASSERT_NE(queue, nullptr);

  mps::MpsCommandBuffer_t buffer = mps::createCommandBuffer(queue);
  ASSERT_NE(buffer, nullptr);

  // Commit and wait
  mps::commit(buffer);
  mps::waitUntilCompleted(buffer);

  // Verify status
  id<MTLCommandBuffer> objc_buffer = (__bridge id<MTLCommandBuffer>)buffer;
  EXPECT_EQ([objc_buffer status], MTLCommandBufferStatusCompleted);

  mps::destroyCommandBuffer(buffer);
  mps::destroyCommandQueue(queue);
}

/**
 * @brief Test that encode_signal_event works.
 */
TEST_F(MpsCommandQueueBufferTest, EncodeSignalEventSucceeds) {
  mps::MpsCommandQueue_t queue = mps::createCommandQueue(device_);
  ASSERT_NE(queue, nullptr);

  mps::MpsEvent_t event = mps::createEvent(device_);
  ASSERT_NE(event, nullptr);

  mps::MpsCommandBuffer_t buffer = mps::createCommandBuffer(queue);
  ASSERT_NE(buffer, nullptr);

  EXPECT_NO_THROW(mps::encodeSignalEvent(buffer, event, 1));

  mps::commit(buffer);
  mps::waitUntilCompleted(buffer);

  mps::destroyCommandBuffer(buffer);
  mps::destroyEvent(event);
  mps::destroyCommandQueue(queue);
}

/**
 * @brief Test that encode_wait works.
 */
TEST_F(MpsCommandQueueBufferTest, EncodeWaitSucceeds) {
  mps::MpsCommandQueue_t queue = mps::createCommandQueue(device_);
  ASSERT_NE(queue, nullptr);

  mps::MpsEvent_t event = mps::createEvent(device_);
  ASSERT_NE(event, nullptr);

  // First, signal the event using explicit command buffer
  {
    mps::MpsCommandBuffer_t buffer_signal = mps::createCommandBuffer(queue);
    ASSERT_NE(buffer_signal, nullptr);
    mps::recordEvent(event, buffer_signal, 1);
    mps::commit(buffer_signal);
    mps::waitUntilCompleted(buffer_signal);
    mps::destroyCommandBuffer(buffer_signal);
  }

  // Then create a buffer that waits for it
  mps::MpsCommandBuffer_t buffer = mps::createCommandBuffer(queue);
  ASSERT_NE(buffer, nullptr);

  EXPECT_NO_THROW(mps::encodeWait(buffer, event, 1));

  mps::commit(buffer);
  mps::waitUntilCompleted(buffer);

  mps::destroyCommandBuffer(buffer);
  mps::destroyEvent(event);
  mps::destroyCommandQueue(queue);
}

/**
 * @brief Test that encode_signal_event with nullptr is handled.
 */
TEST_F(MpsCommandQueueBufferTest, EncodeSignalEventNullEventThrows) {
  mps::MpsCommandQueue_t queue = mps::createCommandQueue(device_);
  ASSERT_NE(queue, nullptr);
  mps::MpsCommandBuffer_t buffer = mps::createCommandBuffer(queue);
  ASSERT_NE(buffer, nullptr);

  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [&] { mps::encodeSignalEvent(buffer, nullptr, 1); });

  mps::destroyCommandBuffer(buffer);
  mps::destroyCommandQueue(queue);
}

TEST_F(MpsCommandQueueBufferTest, EncodeSignalEventNullCommandBufferThrows) {
  mps::MpsEvent_t event = mps::createEvent(device_);
  ASSERT_NE(event, nullptr);
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [&] { mps::encodeSignalEvent(nullptr, event, 1); });
  mps::destroyEvent(event);
}

TEST_F(MpsCommandQueueBufferTest, EncodeWaitNullEventThrows) {
  mps::MpsCommandQueue_t queue = mps::createCommandQueue(device_);
  ASSERT_NE(queue, nullptr);
  mps::MpsCommandBuffer_t buffer = mps::createCommandBuffer(queue);
  ASSERT_NE(buffer, nullptr);

  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [&] { mps::encodeWait(buffer, nullptr, 1); });

  mps::destroyCommandBuffer(buffer);
  mps::destroyCommandQueue(queue);
}

TEST_F(MpsCommandQueueBufferTest, EncodeWaitNullCommandBufferThrows) {
  mps::MpsEvent_t event = mps::createEvent(device_);
  ASSERT_NE(event, nullptr);
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [&] { mps::encodeWait(nullptr, event, 1); });
  mps::destroyEvent(event);
}

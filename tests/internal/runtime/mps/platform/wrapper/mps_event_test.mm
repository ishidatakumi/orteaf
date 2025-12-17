/**
 * @file mps_event_test.mm
 * @brief Tests for MPS/Metal shared event operations.
 */

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include "orteaf/internal/runtime/mps/platform/wrapper/mps_command_buffer.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_command_queue.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_device.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_event.h"

#include "tests/internal/testing/error_assert.h"

#include <gtest/gtest.h>

namespace mps = orteaf::internal::runtime::mps::platform::wrapper;

/**
 * @brief Test fixture for MPS event tests.
 */
class MpsEventTest : public ::testing::Test {
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

  mps::MpsDevice_t device_ = nullptr;
  mps::MpsCommandQueue_t queue_ = nullptr;
};

/**
 * @brief Test that event can be created.
 */
TEST_F(MpsEventTest, CreateEventSucceeds) {
  mps::MpsEvent_t event = mps::createEvent(device_);
  EXPECT_NE(event, nullptr);

  // Verify it's a valid MTLSharedEvent
  id<MTLSharedEvent> objc_event = (__bridge id<MTLSharedEvent>)event;
  EXPECT_NE(objc_event, nil);

  mps::destroyEvent(event);
}

/**
 * @brief Test that event can be destroyed.
 */
TEST_F(MpsEventTest, DestroyEventSucceeds) {
  mps::MpsEvent_t event = mps::createEvent(device_);
  EXPECT_NE(event, nullptr);

  EXPECT_NO_THROW(mps::destroyEvent(event));
}

/**
 * @brief Test that destroy_event with nullptr is ignored.
 */
TEST_F(MpsEventTest, DestroyEventNullptrIsIgnored) {
  EXPECT_NO_THROW(mps::destroyEvent(nullptr));
}

/**
 * @brief Test that event initial value is 0.
 */
TEST_F(MpsEventTest, EventInitialValueIsZero) {
  mps::MpsEvent_t event = mps::createEvent(device_);
  ASSERT_NE(event, nullptr);

  EXPECT_EQ(mps::eventValue(event), 0);

  mps::destroyEvent(event);
}

/**
 * @brief Test that query_event works.
 */
TEST_F(MpsEventTest, QueryEventWorks) {
  mps::MpsEvent_t event = mps::createEvent(device_);
  ASSERT_NE(event, nullptr);

  // Initial value is 0, so query for 1 should be false
  EXPECT_FALSE(mps::queryEvent(event, 1));

  // Query for 0 should be true
  EXPECT_TRUE(mps::queryEvent(event, 0));

  mps::destroyEvent(event);
}

/**
 * @brief Test that record_event works.
 */
TEST_F(MpsEventTest, RecordEventSucceeds) {
  mps::MpsEvent_t event = mps::createEvent(device_);
  ASSERT_NE(event, nullptr);

  mps::MpsCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
  ASSERT_NE(buffer, nullptr);

  EXPECT_NO_THROW(mps::recordEvent(event, buffer, 1));
  mps::commit(buffer);
  mps::waitUntilCompleted(buffer);

  // Event value should be 1 now
  EXPECT_EQ(mps::eventValue(event), 1);
  EXPECT_TRUE(mps::queryEvent(event, 1));

  mps::destroyCommandBuffer(buffer);
  mps::destroyEvent(event);
}

// Removed: write_event convenience function no longer exists

/**
 * @brief Test that wait_event works.
 */
TEST_F(MpsEventTest, WaitEventSucceeds) {
  mps::MpsEvent_t event = mps::createEvent(device_);
  ASSERT_NE(event, nullptr);

  // First signal the event using explicit command buffer
  {
    mps::MpsCommandBuffer_t buffer_signal = mps::createCommandBuffer(queue_);
    ASSERT_NE(buffer_signal, nullptr);
    mps::recordEvent(event, buffer_signal, 1);
    mps::commit(buffer_signal);
    mps::waitUntilCompleted(buffer_signal);
    mps::destroyCommandBuffer(buffer_signal);
  }

  // Then create a buffer that waits for it
  mps::MpsCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
  ASSERT_NE(buffer, nullptr);

  EXPECT_NO_THROW(mps::waitEvent(buffer, event, 1));

  mps::commit(buffer);
  mps::waitUntilCompleted(buffer);

  mps::destroyCommandBuffer(buffer);
  mps::destroyEvent(event);
}

// Removed: wait_event_queue convenience function no longer exists

// Removed: write_event_queue convenience function no longer exists

/**
 * @brief Test that event values can be incremented.
 */
TEST_F(MpsEventTest, EventValuesCanBeIncremented) {
  mps::MpsEvent_t event = mps::createEvent(device_);
  ASSERT_NE(event, nullptr);

  {
    mps::MpsCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
    ASSERT_NE(buffer, nullptr);
    mps::recordEvent(event, buffer, 1);
    mps::commit(buffer);
    mps::waitUntilCompleted(buffer);
    mps::destroyCommandBuffer(buffer);
  }
  EXPECT_EQ(mps::eventValue(event), 1);

  {
    mps::MpsCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
    ASSERT_NE(buffer, nullptr);
    mps::recordEvent(event, buffer, 2);
    mps::commit(buffer);
    mps::waitUntilCompleted(buffer);
    mps::destroyCommandBuffer(buffer);
  }
  EXPECT_EQ(mps::eventValue(event), 2);

  {
    mps::MpsCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
    ASSERT_NE(buffer, nullptr);
    mps::recordEvent(event, buffer, 3);
    mps::commit(buffer);
    mps::waitUntilCompleted(buffer);
    mps::destroyCommandBuffer(buffer);
  }
  EXPECT_EQ(mps::eventValue(event), 3);

  mps::destroyEvent(event);
}

/**
 * @brief Test that query_event checks correctly.
 */
TEST_F(MpsEventTest, QueryEventChecksCorrectly) {
  mps::MpsEvent_t event = mps::createEvent(device_);
  ASSERT_NE(event, nullptr);

  EXPECT_FALSE(mps::queryEvent(event, 1));

  {
    mps::MpsCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
    ASSERT_NE(buffer, nullptr);
    mps::recordEvent(event, buffer, 1);
    mps::commit(buffer);
    mps::waitUntilCompleted(buffer);
    mps::destroyCommandBuffer(buffer);
  }
  EXPECT_TRUE(mps::queryEvent(event, 1));
  EXPECT_FALSE(mps::queryEvent(event, 2));

  {
    mps::MpsCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
    ASSERT_NE(buffer, nullptr);
    mps::recordEvent(event, buffer, 3);
    mps::commit(buffer);
    mps::waitUntilCompleted(buffer);
    mps::destroyCommandBuffer(buffer);
  }
  EXPECT_TRUE(mps::queryEvent(event, 1));
  EXPECT_TRUE(mps::queryEvent(event, 2));
  EXPECT_TRUE(mps::queryEvent(event, 3));
  EXPECT_FALSE(mps::queryEvent(event, 4));

  mps::destroyEvent(event);
}

/**
 * @brief Test that multiple events can be created.
 */
TEST_F(MpsEventTest, CreateMultipleEvents) {
  mps::MpsEvent_t event1 = mps::createEvent(device_);
  mps::MpsEvent_t event2 = mps::createEvent(device_);

  EXPECT_NE(event1, nullptr);
  EXPECT_NE(event2, nullptr);
  EXPECT_NE(event1, event2);

  mps::destroyEvent(event1);
  mps::destroyEvent(event2);
}

/**
 * @brief Test that event can be used across multiple command buffers.
 */
TEST_F(MpsEventTest, EventAcrossMultipleBuffers) {
  mps::MpsEvent_t event = mps::createEvent(device_);
  ASSERT_NE(event, nullptr);

  // Signal from first buffer
  mps::MpsCommandBuffer_t buffer1 = mps::createCommandBuffer(queue_);
  mps::recordEvent(event, buffer1, 1);
  mps::commit(buffer1);

  // Wait in second buffer
  mps::MpsCommandBuffer_t buffer2 = mps::createCommandBuffer(queue_);
  mps::waitEvent(buffer2, event, 1);
  mps::commit(buffer2);

  mps::waitUntilCompleted(buffer1);
  mps::waitUntilCompleted(buffer2);

  EXPECT_EQ(mps::eventValue(event), 1);

  mps::destroyCommandBuffer(buffer1);
  mps::destroyCommandBuffer(buffer2);
  mps::destroyEvent(event);
}

TEST_F(MpsEventTest, RecordEventWithoutCommandBufferUpdatesValue) {
  mps::MpsEvent_t event = mps::createEvent(device_);
  ASSERT_NE(event, nullptr);

  EXPECT_NO_THROW(mps::recordEvent(event, nullptr, 1));
  EXPECT_EQ(mps::eventValue(event), 1);

  mps::destroyEvent(event);
}

TEST_F(MpsEventTest, RecordEventNullptrEventThrows) {
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [] { mps::recordEvent(nullptr, nullptr, 1); });
}

TEST_F(MpsEventTest, QueryEventNullptrThrows) {
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [] { mps::queryEvent(nullptr, 1); });
}

TEST_F(MpsEventTest, EventValueNullptrThrows) {
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [] { mps::eventValue(nullptr); });
}

TEST_F(MpsEventTest, WaitEventNullCommandBufferThrows) {
  mps::MpsEvent_t event = mps::createEvent(device_);
  ASSERT_NE(event, nullptr);
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [&] { mps::waitEvent(nullptr, event, 1); });
  mps::destroyEvent(event);
}

TEST_F(MpsEventTest, WaitEventNullEventThrows) {
  mps::MpsCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
  ASSERT_NE(buffer, nullptr);
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [&] { mps::waitEvent(buffer, nullptr, 1); });
  mps::destroyCommandBuffer(buffer);
}

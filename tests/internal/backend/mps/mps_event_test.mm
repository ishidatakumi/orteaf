/**
 * @file mps_event_test.mm
 * @brief Tests for MPS/Metal shared event operations.
 */

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

#include "orteaf/internal/backend/mps/mps_device.h"
#include "orteaf/internal/backend/mps/mps_command_queue.h"
#include "orteaf/internal/backend/mps/mps_command_buffer.h"
#include "orteaf/internal/backend/mps/mps_event.h"

#include <gtest/gtest.h>

namespace mps = orteaf::internal::backend::mps;

#ifdef ORTEAF_ENABLE_MPS

/**
 * @brief Test fixture for MPS event tests.
 */
class MpsEventTest : public ::testing::Test {
protected:
    void SetUp() override {
        device_ = mps::get_device();
        if (device_ == nullptr) {
            GTEST_SKIP() << "No Metal devices available";
        }
        queue_ = mps::create_command_queue(device_);
        if (queue_ == nullptr) {
            GTEST_SKIP() << "Failed to create command queue";
        }
    }
    
    void TearDown() override {
        if (queue_ != nullptr) {
            mps::destroy_command_queue(queue_);
        }
        if (device_ != nullptr) {
            mps::device_release(device_);
        }
    }
    
    mps::MPSDevice_t device_ = nullptr;
    mps::MPSCommandQueue_t queue_ = nullptr;
};

/**
 * @brief Test that event can be created.
 */
TEST_F(MpsEventTest, CreateEventSucceeds) {
    mps::MPSEvent_t event = mps::create_event(device_);
    EXPECT_NE(event, nullptr);
    
    // Verify it's a valid MTLSharedEvent
    id<MTLSharedEvent> objc_event = (__bridge id<MTLSharedEvent>)event;
    EXPECT_NE(objc_event, nil);
    
    mps::destroy_event(event);
}

/**
 * @brief Test that event can be destroyed.
 */
TEST_F(MpsEventTest, DestroyEventSucceeds) {
    mps::MPSEvent_t event = mps::create_event(device_);
    EXPECT_NE(event, nullptr);
    
    EXPECT_NO_THROW(mps::destroy_event(event));
}

/**
 * @brief Test that destroy_event with nullptr is ignored.
 */
TEST_F(MpsEventTest, DestroyEventNullptrIsIgnored) {
    EXPECT_NO_THROW(mps::destroy_event(nullptr));
}

/**
 * @brief Test that event initial value is 0.
 */
TEST_F(MpsEventTest, EventInitialValueIsZero) {
    mps::MPSEvent_t event = mps::create_event(device_);
    ASSERT_NE(event, nullptr);
    
    EXPECT_EQ(mps::event_value(event), 0);
    
    mps::destroy_event(event);
}

/**
 * @brief Test that query_event works.
 */
TEST_F(MpsEventTest, QueryEventWorks) {
    mps::MPSEvent_t event = mps::create_event(device_);
    ASSERT_NE(event, nullptr);
    
    // Initial value is 0, so query for 1 should be false
    EXPECT_FALSE(mps::query_event(event, 1));
    
    // Query for 0 should be true
    EXPECT_TRUE(mps::query_event(event, 0));
    
    mps::destroy_event(event);
}

/**
 * @brief Test that record_event works.
 */
TEST_F(MpsEventTest, RecordEventSucceeds) {
    mps::MPSEvent_t event = mps::create_event(device_);
    ASSERT_NE(event, nullptr);
    
    mps::MPSCommandBuffer_t buffer = mps::create_command_buffer(queue_);
    ASSERT_NE(buffer, nullptr);
    
    EXPECT_NO_THROW(mps::record_event(event, buffer, 1));
    mps::commit(buffer);
    mps::wait_until_completed(buffer);
    
    // Event value should be 1 now
    EXPECT_EQ(mps::event_value(event), 1);
    EXPECT_TRUE(mps::query_event(event, 1));
    
    mps::destroy_command_buffer(buffer);
    mps::destroy_event(event);
}

// Removed: write_event convenience function no longer exists

/**
 * @brief Test that wait_event works.
 */
TEST_F(MpsEventTest, WaitEventSucceeds) {
    mps::MPSEvent_t event = mps::create_event(device_);
    ASSERT_NE(event, nullptr);
    
    // First signal the event using explicit command buffer
    {
        mps::MPSCommandBuffer_t buffer_signal = mps::create_command_buffer(queue_);
        ASSERT_NE(buffer_signal, nullptr);
        mps::record_event(event, buffer_signal, 1);
        mps::commit(buffer_signal);
        mps::wait_until_completed(buffer_signal);
        mps::destroy_command_buffer(buffer_signal);
    }
    
    // Then create a buffer that waits for it
    mps::MPSCommandBuffer_t buffer = mps::create_command_buffer(queue_);
    ASSERT_NE(buffer, nullptr);
    
    EXPECT_NO_THROW(mps::wait_event(buffer, event, 1));
    
    mps::commit(buffer);
    mps::wait_until_completed(buffer);
    
    mps::destroy_command_buffer(buffer);
    mps::destroy_event(event);
}

// Removed: wait_event_queue convenience function no longer exists

// Removed: write_event_queue convenience function no longer exists

/**
 * @brief Test that event values can be incremented.
 */
TEST_F(MpsEventTest, EventValuesCanBeIncremented) {
    mps::MPSEvent_t event = mps::create_event(device_);
    ASSERT_NE(event, nullptr);
    
    {
        mps::MPSCommandBuffer_t buffer = mps::create_command_buffer(queue_);
        ASSERT_NE(buffer, nullptr);
        mps::record_event(event, buffer, 1);
        mps::commit(buffer);
        mps::wait_until_completed(buffer);
        mps::destroy_command_buffer(buffer);
    }
    EXPECT_EQ(mps::event_value(event), 1);
    
    {
        mps::MPSCommandBuffer_t buffer = mps::create_command_buffer(queue_);
        ASSERT_NE(buffer, nullptr);
        mps::record_event(event, buffer, 2);
        mps::commit(buffer);
        mps::wait_until_completed(buffer);
        mps::destroy_command_buffer(buffer);
    }
    EXPECT_EQ(mps::event_value(event), 2);
    
    {
        mps::MPSCommandBuffer_t buffer = mps::create_command_buffer(queue_);
        ASSERT_NE(buffer, nullptr);
        mps::record_event(event, buffer, 3);
        mps::commit(buffer);
        mps::wait_until_completed(buffer);
        mps::destroy_command_buffer(buffer);
    }
    EXPECT_EQ(mps::event_value(event), 3);
    
    mps::destroy_event(event);
}

/**
 * @brief Test that query_event checks correctly.
 */
TEST_F(MpsEventTest, QueryEventChecksCorrectly) {
    mps::MPSEvent_t event = mps::create_event(device_);
    ASSERT_NE(event, nullptr);
    
    EXPECT_FALSE(mps::query_event(event, 1));
    
    {
        mps::MPSCommandBuffer_t buffer = mps::create_command_buffer(queue_);
        ASSERT_NE(buffer, nullptr);
        mps::record_event(event, buffer, 1);
        mps::commit(buffer);
        mps::wait_until_completed(buffer);
        mps::destroy_command_buffer(buffer);
    }
    EXPECT_TRUE(mps::query_event(event, 1));
    EXPECT_FALSE(mps::query_event(event, 2));
    
    {
        mps::MPSCommandBuffer_t buffer = mps::create_command_buffer(queue_);
        ASSERT_NE(buffer, nullptr);
        mps::record_event(event, buffer, 3);
        mps::commit(buffer);
        mps::wait_until_completed(buffer);
        mps::destroy_command_buffer(buffer);
    }
    EXPECT_TRUE(mps::query_event(event, 1));
    EXPECT_TRUE(mps::query_event(event, 2));
    EXPECT_TRUE(mps::query_event(event, 3));
    EXPECT_FALSE(mps::query_event(event, 4));
    
    mps::destroy_event(event);
}

/**
 * @brief Test that multiple events can be created.
 */
TEST_F(MpsEventTest, CreateMultipleEvents) {
    mps::MPSEvent_t event1 = mps::create_event(device_);
    mps::MPSEvent_t event2 = mps::create_event(device_);
    
    EXPECT_NE(event1, nullptr);
    EXPECT_NE(event2, nullptr);
    EXPECT_NE(event1, event2);
    
    mps::destroy_event(event1);
    mps::destroy_event(event2);
}

/**
 * @brief Test that event can be used across multiple command buffers.
 */
TEST_F(MpsEventTest, EventAcrossMultipleBuffers) {
    mps::MPSEvent_t event = mps::create_event(device_);
    ASSERT_NE(event, nullptr);
    
    // Signal from first buffer
    mps::MPSCommandBuffer_t buffer1 = mps::create_command_buffer(queue_);
    mps::record_event(event, buffer1, 1);
    mps::commit(buffer1);
    
    // Wait in second buffer
    mps::MPSCommandBuffer_t buffer2 = mps::create_command_buffer(queue_);
    mps::wait_event(buffer2, event, 1);
    mps::commit(buffer2);
    
    mps::wait_until_completed(buffer1);
    mps::wait_until_completed(buffer2);
    
    EXPECT_EQ(mps::event_value(event), 1);
    
    mps::destroy_command_buffer(buffer1);
    mps::destroy_command_buffer(buffer2);
    mps::destroy_event(event);
}

/**
 * @brief Test that record_event with nullptr command buffer is handled.
 */
TEST_F(MpsEventTest, RecordEventNullptrBufferHandled) {
    mps::MPSEvent_t event = mps::create_event(device_);
    ASSERT_NE(event, nullptr);
    
    // Should handle nullptr gracefully
    try {
        mps::record_event(event, nullptr, 1);
        // If no exception, event should be set directly
        EXPECT_EQ(mps::event_value(event), 1);
    } catch (...) {
        // Exception is also acceptable
    }
    
    mps::destroy_event(event);
}

#else  // !ORTEAF_ENABLE_MPS

/**
 * @brief Test that event functions return nullptr when MPS is disabled.
 */
TEST(MpsEvent, DisabledReturnsNeutralValues) {
    EXPECT_EQ(mps::create_event(nullptr), nullptr);
    EXPECT_NO_THROW(mps::destroy_event(nullptr));
    EXPECT_NO_THROW(mps::record_event(nullptr, nullptr, 0));
    EXPECT_FALSE(mps::query_event(nullptr, 0));
    EXPECT_EQ(mps::event_value(nullptr), 0);
    EXPECT_NO_THROW(mps::write_event(nullptr, nullptr, 0));
    EXPECT_NO_THROW(mps::wait_event(nullptr, nullptr, 0));
    EXPECT_NO_THROW(mps::write_event_queue(nullptr, nullptr, 0));
    EXPECT_NO_THROW(mps::wait_event_queue(nullptr, nullptr, 0));
}

#endif  // ORTEAF_ENABLE_MPS

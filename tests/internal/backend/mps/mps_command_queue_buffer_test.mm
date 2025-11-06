/**
 * @file mps_command_queue_buffer_test.mm
 * @brief Tests for MPS/Metal command queue and command buffer operations.
 */

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

#include "orteaf/internal/backend/mps/mps_device.h"
#include "orteaf/internal/backend/mps/mps_command_queue.h"
#include "orteaf/internal/backend/mps/mps_command_buffer.h"

#include <gtest/gtest.h>

namespace mps = orteaf::internal::backend::mps;

#ifdef ORTEAF_ENABLE_MPS

/**
 * @brief Test fixture for MPS command queue and buffer tests.
 */
class MpsCommandQueueBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        device_ = mps::get_device();
        if (device_ == nullptr) {
            GTEST_SKIP() << "No Metal devices available";
        }
    }
    
    void TearDown() override {
        if (device_ != nullptr) {
            mps::device_release(device_);
        }
    }
    
    mps::MPSDevice_t device_ = nullptr;
};

/**
 * @brief Test that command queue can be created.
 */
TEST_F(MpsCommandQueueBufferTest, CreateCommandQueueSucceeds) {
    mps::MPSCommandQueue_t queue = mps::create_command_queue(device_);
    EXPECT_NE(queue, nullptr);
    
    // Verify it's a valid MTLCommandQueue
    id<MTLCommandQueue> objc_queue = (__bridge id<MTLCommandQueue>)queue;
    EXPECT_NE(objc_queue, nil);
    
    mps::destroy_command_queue(queue);
}

/**
 * @brief Test that command queue can be destroyed.
 */
TEST_F(MpsCommandQueueBufferTest, DestroyCommandQueueSucceeds) {
    mps::MPSCommandQueue_t queue = mps::create_command_queue(device_);
    EXPECT_NE(queue, nullptr);
    
    EXPECT_NO_THROW(mps::destroy_command_queue(queue));
}

/**
 * @brief Test that destroy_command_queue with nullptr is ignored.
 */
TEST_F(MpsCommandQueueBufferTest, DestroyCommandQueueNullptrIsIgnored) {
    EXPECT_NO_THROW(mps::destroy_command_queue(nullptr));
}

/**
 * @brief Test that multiple command queues can be created.
 */
TEST_F(MpsCommandQueueBufferTest, CreateMultipleCommandQueues) {
    mps::MPSCommandQueue_t queue1 = mps::create_command_queue(device_);
    mps::MPSCommandQueue_t queue2 = mps::create_command_queue(device_);
    
    EXPECT_NE(queue1, nullptr);
    EXPECT_NE(queue2, nullptr);
    EXPECT_NE(queue1, queue2);
    
    mps::destroy_command_queue(queue1);
    mps::destroy_command_queue(queue2);
}

/**
 * @brief Test that command buffer can be created from queue.
 */
TEST_F(MpsCommandQueueBufferTest, CreateCommandBufferSucceeds) {
    mps::MPSCommandQueue_t queue = mps::create_command_queue(device_);
    ASSERT_NE(queue, nullptr);
    
    mps::MPSCommandBuffer_t buffer = mps::create_command_buffer(queue);
    EXPECT_NE(buffer, nullptr);
    
    // Verify it's a valid MTLCommandBuffer
    id<MTLCommandBuffer> objc_buffer = (__bridge id<MTLCommandBuffer>)buffer;
    EXPECT_NE(objc_buffer, nil);
    
    mps::destroy_command_buffer(buffer);
    mps::destroy_command_queue(queue);
}

/**
 * @brief Test that command buffer can be destroyed.
 */
TEST_F(MpsCommandQueueBufferTest, DestroyCommandBufferSucceeds) {
    mps::MPSCommandQueue_t queue = mps::create_command_queue(device_);
    ASSERT_NE(queue, nullptr);
    
    mps::MPSCommandBuffer_t buffer = mps::create_command_buffer(queue);
    ASSERT_NE(buffer, nullptr);
    
    EXPECT_NO_THROW(mps::destroy_command_buffer(buffer));
    mps::destroy_command_queue(queue);
}

/**
 * @brief Test that destroy_command_buffer with nullptr is ignored.
 */
TEST_F(MpsCommandQueueBufferTest, DestroyCommandBufferNullptrIsIgnored) {
    EXPECT_NO_THROW(mps::destroy_command_buffer(nullptr));
}

/**
 * @brief Test that multiple command buffers can be created.
 */
TEST_F(MpsCommandQueueBufferTest, CreateMultipleCommandBuffers) {
    mps::MPSCommandQueue_t queue = mps::create_command_queue(device_);
    ASSERT_NE(queue, nullptr);
    
    mps::MPSCommandBuffer_t buffer1 = mps::create_command_buffer(queue);
    mps::MPSCommandBuffer_t buffer2 = mps::create_command_buffer(queue);
    
    EXPECT_NE(buffer1, nullptr);
    EXPECT_NE(buffer2, nullptr);
    EXPECT_NE(buffer1, buffer2);
    
    mps::destroy_command_buffer(buffer1);
    mps::destroy_command_buffer(buffer2);
    mps::destroy_command_queue(queue);
}

/**
 * @brief Test that command buffer can be committed.
 */
TEST_F(MpsCommandQueueBufferTest, CommitCommandBufferSucceeds) {
    mps::MPSCommandQueue_t queue = mps::create_command_queue(device_);
    ASSERT_NE(queue, nullptr);
    
    mps::MPSCommandBuffer_t buffer = mps::create_command_buffer(queue);
    ASSERT_NE(buffer, nullptr);
    
    EXPECT_NO_THROW(mps::commit(buffer));
    
    mps::destroy_command_buffer(buffer);
    mps::destroy_command_queue(queue);
}

/**
 * @brief Test that command buffer can be waited for completion.
 */
TEST_F(MpsCommandQueueBufferTest, WaitUntilCompletedSucceeds) {
    mps::MPSCommandQueue_t queue = mps::create_command_queue(device_);
    ASSERT_NE(queue, nullptr);
    
    mps::MPSCommandBuffer_t buffer = mps::create_command_buffer(queue);
    ASSERT_NE(buffer, nullptr);
    
    mps::commit(buffer);
    EXPECT_NO_THROW(mps::wait_until_completed(buffer));
    
    mps::destroy_command_buffer(buffer);
    mps::destroy_command_queue(queue);
}

/**
 * @brief Test complete command buffer lifecycle.
 */
TEST_F(MpsCommandQueueBufferTest, CommandBufferLifecycle) {
    mps::MPSCommandQueue_t queue = mps::create_command_queue(device_);
    ASSERT_NE(queue, nullptr);
    
    mps::MPSCommandBuffer_t buffer = mps::create_command_buffer(queue);
    ASSERT_NE(buffer, nullptr);
    
    // Commit and wait
    mps::commit(buffer);
    mps::wait_until_completed(buffer);
    
    // Verify status
    id<MTLCommandBuffer> objc_buffer = (__bridge id<MTLCommandBuffer>)buffer;
    EXPECT_EQ([objc_buffer status], MTLCommandBufferStatusCompleted);
    
    mps::destroy_command_buffer(buffer);
    mps::destroy_command_queue(queue);
}

/**
 * @brief Test that encode_signal_event works.
 */
TEST_F(MpsCommandQueueBufferTest, EncodeSignalEventSucceeds) {
    mps::MPSCommandQueue_t queue = mps::create_command_queue(device_);
    ASSERT_NE(queue, nullptr);
    
    mps::MPSEvent_t event = mps::create_event(device_);
    ASSERT_NE(event, nullptr);
    
    mps::MPSCommandBuffer_t buffer = mps::create_command_buffer(queue);
    ASSERT_NE(buffer, nullptr);
    
    EXPECT_NO_THROW(mps::encode_signal_event(buffer, event, 1));
    
    mps::commit(buffer);
    mps::wait_until_completed(buffer);
    
    mps::destroy_command_buffer(buffer);
    mps::destroy_event(event);
    mps::destroy_command_queue(queue);
}

/**
 * @brief Test that encode_wait works.
 */
TEST_F(MpsCommandQueueBufferTest, EncodeWaitSucceeds) {
    mps::MPSCommandQueue_t queue = mps::create_command_queue(device_);
    ASSERT_NE(queue, nullptr);
    
    mps::MPSEvent_t event = mps::create_event(device_);
    ASSERT_NE(event, nullptr);
    
    // First, signal the event using explicit command buffer
    {
        mps::MPSCommandBuffer_t buffer_signal = mps::create_command_buffer(queue);
        ASSERT_NE(buffer_signal, nullptr);
        mps::record_event(event, buffer_signal, 1);
        mps::commit(buffer_signal);
        mps::wait_until_completed(buffer_signal);
        mps::destroy_command_buffer(buffer_signal);
    }
    
    // Then create a buffer that waits for it
    mps::MPSCommandBuffer_t buffer = mps::create_command_buffer(queue);
    ASSERT_NE(buffer, nullptr);
    
    EXPECT_NO_THROW(mps::encode_wait(buffer, event, 1));
    
    mps::commit(buffer);
    mps::wait_until_completed(buffer);
    
    mps::destroy_command_buffer(buffer);
    mps::destroy_event(event);
    mps::destroy_command_queue(queue);
}

/**
 * @brief Test that encode_signal_event with nullptr is handled.
 */
TEST_F(MpsCommandQueueBufferTest, EncodeSignalEventNullptrHandled) {
    mps::MPSCommandQueue_t queue = mps::create_command_queue(device_);
    ASSERT_NE(queue, nullptr);
    
    mps::MPSCommandBuffer_t buffer = mps::create_command_buffer(queue);
    ASSERT_NE(buffer, nullptr);
    
    // Should handle nullptr gracefully (may throw or ignore)
    try {
        mps::encode_signal_event(buffer, nullptr, 1);
    } catch (...) {
        // Exception is acceptable
    }
    
    mps::destroy_command_buffer(buffer);
    mps::destroy_command_queue(queue);
}

/**
 * @brief Test that encode_wait with nullptr is handled.
 */
TEST_F(MpsCommandQueueBufferTest, EncodeWaitNullptrHandled) {
    mps::MPSCommandQueue_t queue = mps::create_command_queue(device_);
    ASSERT_NE(queue, nullptr);
    
    mps::MPSCommandBuffer_t buffer = mps::create_command_buffer(queue);
    ASSERT_NE(buffer, nullptr);
    
    // Should handle nullptr gracefully (may throw or ignore)
    try {
        mps::encode_wait(buffer, nullptr, 1);
    } catch (...) {
        // Exception is acceptable
    }
    
    mps::destroy_command_buffer(buffer);
    mps::destroy_command_queue(queue);
}

#else  // !ORTEAF_ENABLE_MPS

/**
 * @brief Test that command queue functions return nullptr when MPS is disabled.
 */
TEST(MpsCommandQueueBuffer, DisabledReturnsNeutralValues) {
    EXPECT_EQ(mps::create_command_queue(nullptr), nullptr);
    EXPECT_NO_THROW(mps::destroy_command_queue(nullptr));
    EXPECT_EQ(mps::create_command_buffer(nullptr), nullptr);
    EXPECT_NO_THROW(mps::destroy_command_buffer(nullptr));
    EXPECT_NO_THROW(mps::commit(nullptr));
    EXPECT_NO_THROW(mps::wait_until_completed(nullptr));
    EXPECT_NO_THROW(mps::encode_signal_event(nullptr, nullptr, 0));
    EXPECT_NO_THROW(mps::encode_wait(nullptr, nullptr, 0));
}

#endif  // ORTEAF_ENABLE_MPS

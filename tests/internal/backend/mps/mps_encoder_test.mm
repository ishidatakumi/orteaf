/**
 * @file mps_encoder_test.mm
 * @brief Tests for MPS/Metal compute command encoder operations.
 */

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

#include "orteaf/internal/backend/mps/mps_device.h"
#include "orteaf/internal/backend/mps/mps_command_queue.h"
#include "orteaf/internal/backend/mps/mps_command_buffer.h"
#include "orteaf/internal/backend/mps/mps_buffer.h"
#include "orteaf/internal/backend/mps/mps_compute_command_encorder.h"
#include "orteaf/internal/backend/mps/mps_size.h"
#include "orteaf/internal/backend/mps/mps_library.h"
#include "orteaf/internal/backend/mps/mps_function.h"
#include "orteaf/internal/backend/mps/mps_pipeline_state.h"
#include "orteaf/internal/backend/mps/mps_string.h"
#include "orteaf/internal/backend/mps/mps_compile_options.h"
#include "orteaf/internal/backend/mps/mps_error.h"

#include <gtest/gtest.h>

namespace mps = orteaf::internal::backend::mps;

#ifdef ORTEAF_ENABLE_MPS

/**
 * @brief Test fixture for MPS encoder tests.
 */
class MpsEncoderTest : public ::testing::Test {
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
    
    mps::MPSDevice_t device_ = nullptr;
    mps::MPSCommandQueue_t queue_ = nullptr;
};

/**
 * @brief Test that compute command encoder can be created.
 */
TEST_F(MpsEncoderTest, CreateComputeCommandEncoderSucceeds) {
    mps::MPSCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
    ASSERT_NE(buffer, nullptr);
    
    mps::MPSComputeCommandEncoder_t encoder = mps::createComputeCommandEncoder(buffer);
    EXPECT_NE(encoder, nullptr);
    
    mps::destroyComputeCommandEncoder(encoder);
    mps::destroyCommandBuffer(buffer);
}

/**
 * @brief Test that compute command encoder can be destroyed.
 */
TEST_F(MpsEncoderTest, DestroyComputeCommandEncoderSucceeds) {
    mps::MPSCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
    ASSERT_NE(buffer, nullptr);
    
    mps::MPSComputeCommandEncoder_t encoder = mps::createComputeCommandEncoder(buffer);
    ASSERT_NE(encoder, nullptr);
    
    EXPECT_NO_THROW(mps::destroyComputeCommandEncoder(encoder));
    mps::destroyCommandBuffer(buffer);
}

/**
 * @brief Test that destroy_compute_command_encoder with nullptr is ignored.
 */
TEST_F(MpsEncoderTest, DestroyComputeCommandEncoderNullptrIsIgnored) {
    EXPECT_NO_THROW(mps::destroyComputeCommandEncoder(nullptr));
}

/**
 * @brief Test that end_encoding works.
 */
TEST_F(MpsEncoderTest, EndEncodingSucceeds) {
    mps::MPSCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
    ASSERT_NE(buffer, nullptr);
    
    mps::MPSComputeCommandEncoder_t encoder = mps::createComputeCommandEncoder(buffer);
    ASSERT_NE(encoder, nullptr);
    
    EXPECT_NO_THROW(mps::endEncoding(encoder));
    
    mps::destroyComputeCommandEncoder(encoder);
    mps::destroyCommandBuffer(buffer);
}

/**
 * @brief Test that set_pipeline_state works.
 */
TEST_F(MpsEncoderTest, SetPipelineStateSucceeds) {
    mps::MPSCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
    ASSERT_NE(buffer, nullptr);
    
    mps::MPSComputeCommandEncoder_t encoder = mps::createComputeCommandEncoder(buffer);
    ASSERT_NE(encoder, nullptr);
    
    // Create minimal compute pipeline and bind it
    mps::MPSError_t error = nullptr;
    mps::MPSString_t source = mps::toNsString(std::string_view("kernel void test() {}"));
    mps::MPSCompileOptions_t options = mps::createCompileOptions();
    mps::MPSLibrary_t library = mps::createLibraryWithSource(device_, source, options, &error);
    ASSERT_NE(library, nullptr);
    mps::MPSFunction_t function = mps::createFunction(library, "test");
    ASSERT_NE(function, nullptr);
    mps::MPSPipelineState_t pipeline = mps::createPipelineState(device_, function, &error);
    ASSERT_NE(pipeline, nullptr);

    EXPECT_NO_THROW(mps::setPipelineState(encoder, pipeline));

    mps::endEncoding(encoder);
    mps::destroyComputeCommandEncoder(encoder);
    mps::destroyCommandBuffer(buffer);

    mps::destroyPipelineState(pipeline);
    mps::destroyFunction(function);
    mps::destroyLibrary(library);
    mps::destroyCompileOptions(options);
}

/**
 * @brief Test that set_buffer works.
 */
TEST_F(MpsEncoderTest, SetBufferSucceeds) {
    mps::MPSCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
    ASSERT_NE(buffer, nullptr);
    
    mps::MPSComputeCommandEncoder_t encoder = mps::createComputeCommandEncoder(buffer);
    ASSERT_NE(encoder, nullptr);
    
    mps::MPSBuffer_t mps_buffer = mps::createBuffer(device_, 1024);
    if (mps_buffer != nullptr) {
        EXPECT_NO_THROW(mps::setBuffer(encoder, mps_buffer, 0, 0));
        mps::destroyBuffer(mps_buffer);
    }
    
    mps::endEncoding(encoder);
    mps::destroyComputeCommandEncoder(encoder);
    mps::destroyCommandBuffer(buffer);
}

/**
 * @brief Test that set_buffer with nullptr buffer throws.
 */
TEST_F(MpsEncoderTest, SetBufferNullptrThrows) {
    mps::MPSCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
    ASSERT_NE(buffer, nullptr);
    
    mps::MPSComputeCommandEncoder_t encoder = mps::createComputeCommandEncoder(buffer);
    ASSERT_NE(encoder, nullptr);
    
    EXPECT_THROW(mps::setBuffer(encoder, nullptr, 0, 0), std::system_error);
    
    mps::endEncoding(encoder);
    mps::destroyComputeCommandEncoder(encoder);
    mps::destroyCommandBuffer(buffer);
}

/**
 * @brief Test that set_bytes works.
 */
TEST_F(MpsEncoderTest, SetBytesSucceeds) {
    mps::MPSCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
    ASSERT_NE(buffer, nullptr);
    
    mps::MPSComputeCommandEncoder_t encoder = mps::createComputeCommandEncoder(buffer);
    ASSERT_NE(encoder, nullptr);
    
    const uint32_t data[4] = {1, 2, 3, 4};
    EXPECT_NO_THROW(mps::setBytes(encoder, data, sizeof(data), 0));
    
    mps::endEncoding(encoder);
    mps::destroyComputeCommandEncoder(encoder);
    mps::destroyCommandBuffer(buffer);
}

/**
 * @brief Test that set_threadgroups works.
 */
TEST_F(MpsEncoderTest, SetThreadgroupsSucceeds) {
    mps::MPSCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
    ASSERT_NE(buffer, nullptr);
    
    mps::MPSComputeCommandEncoder_t encoder = mps::createComputeCommandEncoder(buffer);
    ASSERT_NE(encoder, nullptr);
    
    // Create minimal compute pipeline and bind it
    mps::MPSError_t error = nullptr;
    mps::MPSString_t source = mps::toNsString(std::string_view("kernel void test() {}"));
    mps::MPSCompileOptions_t options = mps::createCompileOptions();
    mps::MPSLibrary_t library = mps::createLibraryWithSource(device_, source, options, &error);
    ASSERT_NE(library, nullptr);
    mps::MPSFunction_t function = mps::createFunction(library, "test");
    ASSERT_NE(function, nullptr);
    mps::MPSPipelineState_t pipeline = mps::createPipelineState(device_, function, &error);
    ASSERT_NE(pipeline, nullptr);

    EXPECT_NO_THROW(mps::setPipelineState(encoder, pipeline));

    mps::MPSSize_t threadgroups = mps::makeSize(1, 1, 1);
    mps::MPSSize_t threads_per_threadgroup = mps::makeSize(1, 1, 1);
    
    EXPECT_NO_THROW(mps::setThreadgroups(encoder, threadgroups, threads_per_threadgroup));
    
    mps::endEncoding(encoder);
    mps::destroyComputeCommandEncoder(encoder);
    mps::destroyCommandBuffer(buffer);

    mps::destroyPipelineState(pipeline);
    mps::destroyFunction(function);
    mps::destroyLibrary(library);
    mps::destroyCompileOptions(options);
}

/**
 * @brief Test that encoder can be used with buffer.
 */
TEST_F(MpsEncoderTest, EncoderWithBuffer) {
    mps::MPSCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
    ASSERT_NE(buffer, nullptr);
    
    mps::MPSComputeCommandEncoder_t encoder = mps::createComputeCommandEncoder(buffer);
    ASSERT_NE(encoder, nullptr);
    
    mps::MPSBuffer_t mps_buffer = mps::createBuffer(device_, 256);
    if (mps_buffer != nullptr) {
        mps::setBuffer(encoder, mps_buffer, 0, 0);
        mps::endEncoding(encoder);
        
        mps::commit(buffer);
        mps::waitUntilCompleted(buffer);
        
        mps::destroyBuffer(mps_buffer);
    }
    
    mps::destroyComputeCommandEncoder(encoder);
    mps::destroyCommandBuffer(buffer);
}

/**
 * @brief Test that multiple encoders can be created from same buffer.
 */
TEST_F(MpsEncoderTest, MultipleEncodersFromSameBuffer) {
    mps::MPSCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
    ASSERT_NE(buffer, nullptr);
    
    mps::MPSComputeCommandEncoder_t encoder1 = mps::createComputeCommandEncoder(buffer);
    EXPECT_NE(encoder1, nullptr);
    mps::endEncoding(encoder1);
    mps::destroyComputeCommandEncoder(encoder1);
    
    mps::MPSComputeCommandEncoder_t encoder2 = mps::createComputeCommandEncoder(buffer);
    EXPECT_NE(encoder2, nullptr);
    mps::endEncoding(encoder2);
    mps::destroyComputeCommandEncoder(encoder2);
    
    mps::destroyCommandBuffer(buffer);
}

#else  // !ORTEAF_ENABLE_MPS

/**
 * @brief Test that encoder functions return nullptr when MPS is disabled.
 */
TEST(MpsEncoder, DisabledReturnsNeutralValues) {
    EXPECT_EQ(mps::createComputeCommandEncoder(nullptr), nullptr);
    EXPECT_NO_THROW(mps::destroyComputeCommandEncoder(nullptr));
    EXPECT_NO_THROW(mps::endEncoding(nullptr));
    EXPECT_NO_THROW(mps::setPipelineState(nullptr, nullptr));
    EXPECT_NO_THROW(mps::setBuffer(nullptr, nullptr, 0, 0));
    EXPECT_NO_THROW(mps::setBytes(nullptr, nullptr, 0, 0));
    
    mps::MPSSize_t size = mps::makeSize(1, 1, 1);
    EXPECT_NO_THROW(mps::setThreadgroups(nullptr, size, size));
}

#endif  // ORTEAF_ENABLE_MPS

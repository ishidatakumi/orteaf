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
 * @brief Test that compute command encoder can be created.
 */
TEST_F(MpsEncoderTest, CreateComputeCommandEncoderSucceeds) {
    mps::MPSCommandBuffer_t buffer = mps::create_command_buffer(queue_);
    ASSERT_NE(buffer, nullptr);
    
    mps::MPSComputeCommandEncoder_t encoder = mps::create_compute_command_encoder(buffer);
    EXPECT_NE(encoder, nullptr);
    
    mps::destroy_compute_command_encoder(encoder);
    mps::destroy_command_buffer(buffer);
}

/**
 * @brief Test that compute command encoder can be destroyed.
 */
TEST_F(MpsEncoderTest, DestroyComputeCommandEncoderSucceeds) {
    mps::MPSCommandBuffer_t buffer = mps::create_command_buffer(queue_);
    ASSERT_NE(buffer, nullptr);
    
    mps::MPSComputeCommandEncoder_t encoder = mps::create_compute_command_encoder(buffer);
    ASSERT_NE(encoder, nullptr);
    
    EXPECT_NO_THROW(mps::destroy_compute_command_encoder(encoder));
    mps::destroy_command_buffer(buffer);
}

/**
 * @brief Test that destroy_compute_command_encoder with nullptr is ignored.
 */
TEST_F(MpsEncoderTest, DestroyComputeCommandEncoderNullptrIsIgnored) {
    EXPECT_NO_THROW(mps::destroy_compute_command_encoder(nullptr));
}

/**
 * @brief Test that end_encoding works.
 */
TEST_F(MpsEncoderTest, EndEncodingSucceeds) {
    mps::MPSCommandBuffer_t buffer = mps::create_command_buffer(queue_);
    ASSERT_NE(buffer, nullptr);
    
    mps::MPSComputeCommandEncoder_t encoder = mps::create_compute_command_encoder(buffer);
    ASSERT_NE(encoder, nullptr);
    
    EXPECT_NO_THROW(mps::end_encoding(encoder));
    
    mps::destroy_compute_command_encoder(encoder);
    mps::destroy_command_buffer(buffer);
}

/**
 * @brief Test that set_pipeline_state works.
 */
TEST_F(MpsEncoderTest, SetPipelineStateSucceeds) {
    mps::MPSCommandBuffer_t buffer = mps::create_command_buffer(queue_);
    ASSERT_NE(buffer, nullptr);
    
    mps::MPSComputeCommandEncoder_t encoder = mps::create_compute_command_encoder(buffer);
    ASSERT_NE(encoder, nullptr);
    
    // Create minimal compute pipeline and bind it
    mps::MPSError_t error = nullptr;
    mps::MPSString_t source = mps::to_ns_string(std::string_view("kernel void test() {}"));
    mps::MPSCompileOptions_t options = mps::create_compile_options();
    mps::MPSLibrary_t library = mps::create_library_with_source(device_, source, options, &error);
    ASSERT_NE(library, nullptr);
    mps::MPSFunction_t function = mps::create_function(library, "test");
    ASSERT_NE(function, nullptr);
    mps::MPSPipelineState_t pipeline = mps::create_pipeline_state(device_, function, &error);
    ASSERT_NE(pipeline, nullptr);

    EXPECT_NO_THROW(mps::set_pipeline_state(encoder, pipeline));

    mps::end_encoding(encoder);
    mps::destroy_compute_command_encoder(encoder);
    mps::destroy_command_buffer(buffer);

    mps::destroy_pipeline_state(pipeline);
    mps::destroy_function(function);
    mps::destroy_library(library);
    mps::destroy_compile_options(options);
}

/**
 * @brief Test that set_buffer works.
 */
TEST_F(MpsEncoderTest, SetBufferSucceeds) {
    mps::MPSCommandBuffer_t buffer = mps::create_command_buffer(queue_);
    ASSERT_NE(buffer, nullptr);
    
    mps::MPSComputeCommandEncoder_t encoder = mps::create_compute_command_encoder(buffer);
    ASSERT_NE(encoder, nullptr);
    
    mps::MPSBuffer_t mps_buffer = mps::create_buffer(device_, 1024);
    if (mps_buffer != nullptr) {
        EXPECT_NO_THROW(mps::set_buffer(encoder, mps_buffer, 0, 0));
        mps::destroy_buffer(mps_buffer);
    }
    
    mps::end_encoding(encoder);
    mps::destroy_compute_command_encoder(encoder);
    mps::destroy_command_buffer(buffer);
}

/**
 * @brief Test that set_buffer with nullptr buffer throws.
 */
TEST_F(MpsEncoderTest, SetBufferNullptrThrows) {
    mps::MPSCommandBuffer_t buffer = mps::create_command_buffer(queue_);
    ASSERT_NE(buffer, nullptr);
    
    mps::MPSComputeCommandEncoder_t encoder = mps::create_compute_command_encoder(buffer);
    ASSERT_NE(encoder, nullptr);
    
    EXPECT_THROW(mps::set_buffer(encoder, nullptr, 0, 0), std::system_error);
    
    mps::end_encoding(encoder);
    mps::destroy_compute_command_encoder(encoder);
    mps::destroy_command_buffer(buffer);
}

/**
 * @brief Test that set_bytes works.
 */
TEST_F(MpsEncoderTest, SetBytesSucceeds) {
    mps::MPSCommandBuffer_t buffer = mps::create_command_buffer(queue_);
    ASSERT_NE(buffer, nullptr);
    
    mps::MPSComputeCommandEncoder_t encoder = mps::create_compute_command_encoder(buffer);
    ASSERT_NE(encoder, nullptr);
    
    const uint32_t data[4] = {1, 2, 3, 4};
    EXPECT_NO_THROW(mps::set_bytes(encoder, data, sizeof(data), 0));
    
    mps::end_encoding(encoder);
    mps::destroy_compute_command_encoder(encoder);
    mps::destroy_command_buffer(buffer);
}

/**
 * @brief Test that set_threadgroups works.
 */
TEST_F(MpsEncoderTest, SetThreadgroupsSucceeds) {
    mps::MPSCommandBuffer_t buffer = mps::create_command_buffer(queue_);
    ASSERT_NE(buffer, nullptr);
    
    mps::MPSComputeCommandEncoder_t encoder = mps::create_compute_command_encoder(buffer);
    ASSERT_NE(encoder, nullptr);
    
    // Create minimal compute pipeline and bind it
    mps::MPSError_t error = nullptr;
    mps::MPSString_t source = mps::to_ns_string(std::string_view("kernel void test() {}"));
    mps::MPSCompileOptions_t options = mps::create_compile_options();
    mps::MPSLibrary_t library = mps::create_library_with_source(device_, source, options, &error);
    ASSERT_NE(library, nullptr);
    mps::MPSFunction_t function = mps::create_function(library, "test");
    ASSERT_NE(function, nullptr);
    mps::MPSPipelineState_t pipeline = mps::create_pipeline_state(device_, function, &error);
    ASSERT_NE(pipeline, nullptr);

    EXPECT_NO_THROW(mps::set_pipeline_state(encoder, pipeline));

    mps::MPSSize_t threadgroups = mps::make_size(1, 1, 1);
    mps::MPSSize_t threads_per_threadgroup = mps::make_size(1, 1, 1);
    
    EXPECT_NO_THROW(mps::set_threadgroups(encoder, threadgroups, threads_per_threadgroup));
    
    mps::end_encoding(encoder);
    mps::destroy_compute_command_encoder(encoder);
    mps::destroy_command_buffer(buffer);

    mps::destroy_pipeline_state(pipeline);
    mps::destroy_function(function);
    mps::destroy_library(library);
    mps::destroy_compile_options(options);
}

/**
 * @brief Test that encoder can be used with buffer.
 */
TEST_F(MpsEncoderTest, EncoderWithBuffer) {
    mps::MPSCommandBuffer_t buffer = mps::create_command_buffer(queue_);
    ASSERT_NE(buffer, nullptr);
    
    mps::MPSComputeCommandEncoder_t encoder = mps::create_compute_command_encoder(buffer);
    ASSERT_NE(encoder, nullptr);
    
    mps::MPSBuffer_t mps_buffer = mps::create_buffer(device_, 256);
    if (mps_buffer != nullptr) {
        mps::set_buffer(encoder, mps_buffer, 0, 0);
        mps::end_encoding(encoder);
        
        mps::commit(buffer);
        mps::wait_until_completed(buffer);
        
        mps::destroy_buffer(mps_buffer);
    }
    
    mps::destroy_compute_command_encoder(encoder);
    mps::destroy_command_buffer(buffer);
}

/**
 * @brief Test that multiple encoders can be created from same buffer.
 */
TEST_F(MpsEncoderTest, MultipleEncodersFromSameBuffer) {
    mps::MPSCommandBuffer_t buffer = mps::create_command_buffer(queue_);
    ASSERT_NE(buffer, nullptr);
    
    mps::MPSComputeCommandEncoder_t encoder1 = mps::create_compute_command_encoder(buffer);
    EXPECT_NE(encoder1, nullptr);
    mps::end_encoding(encoder1);
    mps::destroy_compute_command_encoder(encoder1);
    
    mps::MPSComputeCommandEncoder_t encoder2 = mps::create_compute_command_encoder(buffer);
    EXPECT_NE(encoder2, nullptr);
    mps::end_encoding(encoder2);
    mps::destroy_compute_command_encoder(encoder2);
    
    mps::destroy_command_buffer(buffer);
}

#else  // !ORTEAF_ENABLE_MPS

/**
 * @brief Test that encoder functions return nullptr when MPS is disabled.
 */
TEST(MpsEncoder, DisabledReturnsNeutralValues) {
    EXPECT_EQ(mps::create_compute_command_encoder(nullptr), nullptr);
    EXPECT_NO_THROW(mps::destroy_compute_command_encoder(nullptr));
    EXPECT_NO_THROW(mps::end_encoding(nullptr));
    EXPECT_NO_THROW(mps::set_pipeline_state(nullptr, nullptr));
    EXPECT_NO_THROW(mps::set_buffer(nullptr, nullptr, 0, 0));
    EXPECT_NO_THROW(mps::set_bytes(nullptr, nullptr, 0, 0));
    
    mps::MPSSize_t size = mps::make_size(1, 1, 1);
    EXPECT_NO_THROW(mps::set_threadgroups(nullptr, size, size));
}

#endif  // ORTEAF_ENABLE_MPS

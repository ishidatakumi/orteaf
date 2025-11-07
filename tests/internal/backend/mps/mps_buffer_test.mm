/**
 * @file mps_buffer_test.mm
 * @brief Tests for MPS/Metal buffer operations.
 */

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

#include "orteaf/internal/backend/mps/mps_device.h"
#include "orteaf/internal/backend/mps/mps_buffer.h"

#include <gtest/gtest.h>

namespace mps = orteaf::internal::backend::mps;

#ifdef ORTEAF_ENABLE_MPS

/**
 * @brief Test fixture for MPS buffer tests.
 */
class MpsBufferTest : public ::testing::Test {
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
 * @brief Test that buffer creation succeeds.
 */
TEST_F(MpsBufferTest, CreateBufferSucceeds) {
    mps::MPSBuffer_t buffer = mps::create_buffer(device_, 1024, 0);
    EXPECT_NE(buffer, nullptr);
    
    mps::destroy_buffer(buffer);
}

/**
 * @brief Test that create_buffer with nullptr device throws.
 */
TEST_F(MpsBufferTest, CreateBufferNullptrDeviceThrows) {
    EXPECT_THROW(mps::create_buffer(nullptr, 1024, 0), std::system_error);
}

/**
 * @brief Test that create_buffer with zero size throws.
 */
TEST_F(MpsBufferTest, CreateBufferZeroSizeThrows) {
    EXPECT_THROW(mps::create_buffer(device_, 0, 0), std::system_error);
}

/**
 * @brief Test that buffer destruction works.
 */
TEST_F(MpsBufferTest, DestroyBufferSucceeds) {
    mps::MPSBuffer_t buffer = mps::create_buffer(device_, 1024, 0);
    ASSERT_NE(buffer, nullptr);
    
    EXPECT_NO_THROW(mps::destroy_buffer(buffer));
}

/**
 * @brief Test that destroy_buffer with nullptr is ignored.
 */
TEST_F(MpsBufferTest, DestroyBufferNullptrIsIgnored) {
    EXPECT_NO_THROW(mps::destroy_buffer(nullptr));
}

/**
 * @brief Test that multiple buffers can be created.
 */
TEST_F(MpsBufferTest, CreateMultipleBuffers) {
    mps::MPSBuffer_t buffer1 = mps::create_buffer(device_, 256, 0);
    mps::MPSBuffer_t buffer2 = mps::create_buffer(device_, 512, 0);
    
    EXPECT_NE(buffer1, nullptr);
    EXPECT_NE(buffer2, nullptr);
    EXPECT_NE(buffer1, buffer2);
    
    mps::destroy_buffer(buffer1);
    mps::destroy_buffer(buffer2);
}

/**
 * @brief Test that buffer contents can be accessed.
 */
TEST_F(MpsBufferTest, GetBufferContentsSucceeds) {
    mps::MPSBuffer_t buffer = mps::create_buffer(device_, 1024, 0);
    ASSERT_NE(buffer, nullptr);
    
    const void* contents = mps::get_buffer_contents_const(buffer);
    EXPECT_NE(contents, nullptr);
    
    mps::destroy_buffer(buffer);
}

/**
 * @brief Test that buffer contents with nullptr returns nullptr.
 */
TEST_F(MpsBufferTest, GetBufferContentsNullptrReturnsNullptr) {
    EXPECT_EQ(mps::get_buffer_contents_const(nullptr), nullptr);
}

#else  // !ORTEAF_ENABLE_MPS

/**
 * @brief Test that buffer functions return nullptr when MPS is disabled.
 */
TEST(MpsBuffer, DisabledReturnsNeutralValues) {
    EXPECT_EQ(mps::create_buffer(nullptr, 1024, 0), nullptr);
    EXPECT_NO_THROW(mps::destroy_buffer(nullptr));
    EXPECT_EQ(mps::get_buffer_contents_const(nullptr), nullptr);
}

#endif  // ORTEAF_ENABLE_MPS


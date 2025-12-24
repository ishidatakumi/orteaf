/**
 * @file mps_buffer_test.mm
 * @brief Tests for MPS/Metal buffer operations.
 */

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include "orteaf/internal/execution/mps/platform/wrapper/mps_buffer.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_device.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_heap.h"

#include "tests/internal/testing/error_assert.h"

#include <exception>
#include <gtest/gtest.h>

namespace mps = orteaf::internal::execution::mps::platform::wrapper;

/**
 * @brief Test fixture for MPS buffer tests.
 */
class MpsBufferTest : public ::testing::Test {
protected:
  void SetUp() override {
    device_ = mps::getDevice();
    if (device_ == nullptr) {
      GTEST_SKIP() << "No Metal devices available";
    }
    heap_descriptor_ = mps::createHeapDescriptor();
    if (heap_descriptor_ == nullptr) {
      GTEST_SKIP() << "Failed to create heap descriptor";
    }
    try {
      mps::setHeapDescriptorSize(heap_descriptor_, 1 << 20);
      heap_ = mps::createHeap(device_, heap_descriptor_);
    } catch (const std::exception &ex) {
      GTEST_SKIP() << "Failed to configure heap: " << ex.what();
    }
  }

  void TearDown() override {
    if (heap_ != nullptr) {
      mps::destroyHeap(heap_);
      heap_ = nullptr;
    }
    if (heap_descriptor_ != nullptr) {
      mps::destroyHeapDescriptor(heap_descriptor_);
      heap_descriptor_ = nullptr;
    }
    if (device_ != nullptr) {
      mps::deviceRelease(device_);
      device_ = nullptr;
    }
  }

  mps::MpsDevice_t device_ = nullptr;
  mps::MpsHeapDescriptor_t heap_descriptor_ = nullptr;
  mps::MpsHeap_t heap_ = nullptr;
};

/**
 * @brief Test that buffer creation succeeds.
 */
TEST_F(MpsBufferTest, CreateBufferSucceeds) {
  mps::MpsBuffer_t buffer = mps::createBuffer(heap_, 1024);
  EXPECT_NE(buffer, nullptr);

  mps::destroyBuffer(buffer);
}

/**
 * @brief Test that create_buffer with nullptr device throws.
 */
TEST_F(MpsBufferTest, CreateBufferNullptrHeapThrows) {
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [] { mps::createBuffer(nullptr, 1024); });
}

/**
 * @brief Test that create_buffer with zero size throws.
 */
TEST_F(MpsBufferTest, CreateBufferZeroSizeThrows) {
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
      [&] { mps::createBuffer(heap_, 0); });
}

/**
 * @brief Test that buffer destruction works.
 */
TEST_F(MpsBufferTest, DestroyBufferSucceeds) {
  mps::MpsBuffer_t buffer = mps::createBuffer(heap_, 1024);
  ASSERT_NE(buffer, nullptr);

  EXPECT_NO_THROW(mps::destroyBuffer(buffer));
}

/**
 * @brief Test that destroy_buffer with nullptr is ignored.
 */
TEST_F(MpsBufferTest, DestroyBufferNullptrIsIgnored) {
  EXPECT_NO_THROW(mps::destroyBuffer(nullptr));
}

/**
 * @brief Test that multiple buffers can be created.
 */
TEST_F(MpsBufferTest, CreateMultipleBuffers) {
  mps::MpsBuffer_t buffer1 = mps::createBuffer(heap_, 256);
  mps::MpsBuffer_t buffer2 = mps::createBuffer(heap_, 512);

  EXPECT_NE(buffer1, nullptr);
  EXPECT_NE(buffer2, nullptr);
  EXPECT_NE(buffer1, buffer2);

  mps::destroyBuffer(buffer1);
  mps::destroyBuffer(buffer2);
}

/**
 * @brief Test that buffer contents can be accessed.
 */
TEST_F(MpsBufferTest, GetBufferContentsSucceeds) {
  mps::MpsBuffer_t buffer = mps::createBuffer(heap_, 1024);
  ASSERT_NE(buffer, nullptr);

  const void *contents = mps::getBufferContentsConst(buffer);
  EXPECT_NE(contents, nullptr);

  mps::destroyBuffer(buffer);
}

/**
 * @brief Test that buffer contents with nullptr returns nullptr.
 */
TEST_F(MpsBufferTest, GetBufferContentsNullptrReturnsNullptr) {
  EXPECT_EQ(mps::getBufferContentsConst(nullptr), nullptr);
}

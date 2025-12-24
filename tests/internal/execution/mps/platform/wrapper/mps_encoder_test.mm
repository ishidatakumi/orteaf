/**
 * @file mps_encoder_test.mm
 * @brief Tests for MPS/Metal compute command encoder operations.
 */

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include "orteaf/internal/execution/mps/platform/wrapper/mps_buffer.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_command_buffer.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_command_queue.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_compile_options.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_compute_command_encoder.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_compute_pipeline_state.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_device.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_error.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_function.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_heap.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_library.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_size.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_string.h"

#include "tests/internal/testing/error_assert.h"

#include <exception>
#include <gtest/gtest.h>

namespace mps = orteaf::internal::execution::mps::platform::wrapper;

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
    if (queue_ != nullptr) {
      mps::destroyCommandQueue(queue_);
    }
    if (device_ != nullptr) {
      mps::deviceRelease(device_);
    }
  }

  mps::MpsDevice_t device_ = nullptr;
  mps::MpsCommandQueue_t queue_ = nullptr;
  mps::MpsHeapDescriptor_t heap_descriptor_ = nullptr;
  mps::MpsHeap_t heap_ = nullptr;
};

/**
 * @brief Test that compute command encoder can be created.
 */
TEST_F(MpsEncoderTest, CreateComputeCommandEncoderSucceeds) {
  mps::MpsCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
  ASSERT_NE(buffer, nullptr);

  mps::MpsComputeCommandEncoder_t encoder =
      mps::createComputeCommandEncoder(buffer);
  EXPECT_NE(encoder, nullptr);

  mps::destroyComputeCommandEncoder(encoder);
  mps::destroyCommandBuffer(buffer);
}

/**
 * @brief Test that compute command encoder can be destroyed.
 */
TEST_F(MpsEncoderTest, DestroyComputeCommandEncoderSucceeds) {
  mps::MpsCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
  ASSERT_NE(buffer, nullptr);

  mps::MpsComputeCommandEncoder_t encoder =
      mps::createComputeCommandEncoder(buffer);
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
  mps::MpsCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
  ASSERT_NE(buffer, nullptr);

  mps::MpsComputeCommandEncoder_t encoder =
      mps::createComputeCommandEncoder(buffer);
  ASSERT_NE(encoder, nullptr);

  EXPECT_NO_THROW(mps::endEncoding(encoder));

  mps::destroyComputeCommandEncoder(encoder);
  mps::destroyCommandBuffer(buffer);
}

/**
 * @brief Test that set_pipeline_state works.
 */
TEST_F(MpsEncoderTest, SetPipelineStateSucceeds) {
  mps::MpsCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
  ASSERT_NE(buffer, nullptr);

  mps::MpsComputeCommandEncoder_t encoder =
      mps::createComputeCommandEncoder(buffer);
  ASSERT_NE(encoder, nullptr);

  // Create minimal compute pipeline and bind it
  mps::MpsError_t error = nullptr;
  mps::MpsString_t source =
      mps::toNsString(std::string_view("kernel void test() {}"));
  mps::MpsCompileOptions_t options = mps::createCompileOptions();
  mps::MpsLibrary_t library =
      mps::createLibraryWithSource(device_, source, options, &error);
  ASSERT_NE(library, nullptr);
  mps::MpsFunction_t function = mps::createFunction(library, "test");
  ASSERT_NE(function, nullptr);
  mps::MpsComputePipelineState_t pipeline =
      mps::createComputePipelineState(device_, function, &error);
  ASSERT_NE(pipeline, nullptr);

  EXPECT_NO_THROW(mps::setPipelineState(encoder, pipeline));

  mps::endEncoding(encoder);
  mps::destroyComputeCommandEncoder(encoder);
  mps::destroyCommandBuffer(buffer);

  mps::destroyComputePipelineState(pipeline);
  mps::destroyFunction(function);
  mps::destroyLibrary(library);
  mps::destroyCompileOptions(options);
}

/**
 * @brief Test that set_buffer works.
 */
TEST_F(MpsEncoderTest, SetBufferSucceeds) {
  mps::MpsCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
  ASSERT_NE(buffer, nullptr);

  mps::MpsComputeCommandEncoder_t encoder =
      mps::createComputeCommandEncoder(buffer);
  ASSERT_NE(encoder, nullptr);

  mps::MpsBuffer_t mps_buffer = mps::createBuffer(heap_, 1024);
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
  mps::MpsCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
  ASSERT_NE(buffer, nullptr);

  mps::MpsComputeCommandEncoder_t encoder =
      mps::createComputeCommandEncoder(buffer);
  ASSERT_NE(encoder, nullptr);

  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [&] { mps::setBuffer(encoder, nullptr, 0, 0); });

  mps::endEncoding(encoder);
  mps::destroyComputeCommandEncoder(encoder);
  mps::destroyCommandBuffer(buffer);
}

/**
 * @brief Test that set_bytes works.
 */
TEST_F(MpsEncoderTest, SetBytesSucceeds) {
  mps::MpsCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
  ASSERT_NE(buffer, nullptr);

  mps::MpsComputeCommandEncoder_t encoder =
      mps::createComputeCommandEncoder(buffer);
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
  mps::MpsCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
  ASSERT_NE(buffer, nullptr);

  mps::MpsComputeCommandEncoder_t encoder =
      mps::createComputeCommandEncoder(buffer);
  ASSERT_NE(encoder, nullptr);

  // Create minimal compute pipeline and bind it
  mps::MpsError_t error = nullptr;
  mps::MpsString_t source =
      mps::toNsString(std::string_view("kernel void test() {}"));
  mps::MpsCompileOptions_t options = mps::createCompileOptions();
  mps::MpsLibrary_t library =
      mps::createLibraryWithSource(device_, source, options, &error);
  ASSERT_NE(library, nullptr);
  mps::MpsFunction_t function = mps::createFunction(library, "test");
  ASSERT_NE(function, nullptr);
  mps::MpsComputePipelineState_t pipeline =
      mps::createComputePipelineState(device_, function, &error);
  ASSERT_NE(pipeline, nullptr);

  EXPECT_NO_THROW(mps::setPipelineState(encoder, pipeline));

  mps::MPSSize_t threadgroups = mps::makeSize(1, 1, 1);
  mps::MPSSize_t threads_per_threadgroup = mps::makeSize(1, 1, 1);

  EXPECT_NO_THROW(
      mps::setThreadgroups(encoder, threadgroups, threads_per_threadgroup));

  mps::endEncoding(encoder);
  mps::destroyComputeCommandEncoder(encoder);
  mps::destroyCommandBuffer(buffer);

  mps::destroyComputePipelineState(pipeline);
  mps::destroyFunction(function);
  mps::destroyLibrary(library);
  mps::destroyCompileOptions(options);
}

/**
 * @brief Test that encoder can be used with buffer.
 */
TEST_F(MpsEncoderTest, EncoderWithBuffer) {
  mps::MpsCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
  ASSERT_NE(buffer, nullptr);

  mps::MpsComputeCommandEncoder_t encoder =
      mps::createComputeCommandEncoder(buffer);
  ASSERT_NE(encoder, nullptr);

  mps::MpsBuffer_t mps_buffer = mps::createBuffer(heap_, 256);
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
  mps::MpsCommandBuffer_t buffer = mps::createCommandBuffer(queue_);
  ASSERT_NE(buffer, nullptr);

  mps::MpsComputeCommandEncoder_t encoder1 =
      mps::createComputeCommandEncoder(buffer);
  EXPECT_NE(encoder1, nullptr);
  mps::endEncoding(encoder1);
  mps::destroyComputeCommandEncoder(encoder1);

  mps::MpsComputeCommandEncoder_t encoder2 =
      mps::createComputeCommandEncoder(buffer);
  EXPECT_NE(encoder2, nullptr);
  mps::endEncoding(encoder2);
  mps::destroyComputeCommandEncoder(encoder2);

  mps::destroyCommandBuffer(buffer);
}

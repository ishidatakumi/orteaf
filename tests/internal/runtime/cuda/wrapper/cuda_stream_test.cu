/**
 * @file cuda_stream_test.cpp
 * @brief Tests for CUDA stream creation, synchronization, and signaling helpers.
 */

#include "orteaf/internal/runtime/cuda/platform/wrapper/cuda_stream.h"
#include "orteaf/internal/runtime/cuda/platform/wrapper/cuda_context.h"
#include "orteaf/internal/runtime/cuda/platform/wrapper/cuda_init.h"
#include "orteaf/internal/runtime/cuda/platform/wrapper/cuda_device.h"
#include "orteaf/internal/runtime/cuda/platform/wrapper/cuda_alloc.h"
#include "tests/internal/testing/error_assert.h"

#include <gtest/gtest.h>

namespace cuda = orteaf::internal::runtime::cuda::platform::wrapper;

class CudaStreamTest : public ::testing::Test {
protected:
    void SetUp() override {
        cuda::cudaInit();
        int count = cuda::getDeviceCount();
        if (count == 0) {
            GTEST_SKIP() << "No CUDA devices available";
        }
        device_ = cuda::getDevice(0);
        context_ = cuda::getPrimaryContext(device_);
        EXPECT_NE(context_, nullptr);
        cuda::setContext(context_);
    }

    void TearDown() override {
        if (context_ != nullptr) {
            cuda::releasePrimaryContext(device_);
        }
    }

    cuda::CudaDevice_t device_{0};
    cuda::CudaContext_t context_ = nullptr;
};

TEST_F(CudaStreamTest, GetStreamSucceeds) {
    cuda::CudaStream_t stream = cuda::getStream();
    EXPECT_NE(stream, nullptr);
    cuda::releaseStream(stream);
}

TEST_F(CudaStreamTest, CreateMultipleStreams) {
    cuda::CudaStream_t stream1 = cuda::getStream();
    cuda::CudaStream_t stream2 = cuda::getStream();
    EXPECT_NE(stream1, nullptr);
    EXPECT_NE(stream2, nullptr);
    EXPECT_NE(stream1, stream2);
    cuda::releaseStream(stream1);
    cuda::releaseStream(stream2);
}

TEST_F(CudaStreamTest, ReleaseStreamSucceeds) {
    cuda::CudaStream_t stream = cuda::getStream();
    EXPECT_NO_THROW(cuda::releaseStream(stream));
}

TEST_F(CudaStreamTest, ReleaseStreamNullptrNoOp) {
    EXPECT_NO_THROW(cuda::releaseStream(nullptr));
}

TEST_F(CudaStreamTest, SynchronizeStreamSucceeds) {
    cuda::CudaStream_t stream = cuda::getStream();
    EXPECT_NO_THROW(cuda::synchronizeStream(stream));
    cuda::releaseStream(stream);
}

TEST_F(CudaStreamTest, SynchronizeStreamNullptrThrows) {
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        []() { cuda::synchronizeStream(nullptr); });
}

TEST_F(CudaStreamTest, WaitStreamSucceeds) {
    cuda::CudaStream_t stream = cuda::getStream();
    constexpr size_t size = sizeof(uint32_t);
    cuda::CudaDevicePtr_t dev_ptr = cuda::alloc(size);
    EXPECT_NE(dev_ptr, 0);
    EXPECT_NO_THROW(cuda::writeStream(stream, dev_ptr, 42));
    EXPECT_NO_THROW(cuda::waitStream(stream, dev_ptr, 42));
    cuda::free(dev_ptr, size);
    cuda::releaseStream(stream);
}

TEST_F(CudaStreamTest, WriteStreamSucceeds) {
    cuda::CudaStream_t stream = cuda::getStream();
    constexpr size_t size = sizeof(uint32_t);
    cuda::CudaDevicePtr_t dev_ptr = cuda::alloc(size);
    EXPECT_NE(dev_ptr, 0);
    EXPECT_NO_THROW(cuda::writeStream(stream, dev_ptr, 123));
    cuda::free(dev_ptr, size);
    cuda::releaseStream(stream);
}

TEST_F(CudaStreamTest, WaitStreamNullptrThrows) {
    constexpr size_t size = sizeof(uint32_t);
    cuda::CudaDevicePtr_t dev_ptr = cuda::alloc(size);
    EXPECT_NE(dev_ptr, 0);
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        [&]() { cuda::waitStream(nullptr, dev_ptr, 42); });
    cuda::free(dev_ptr, size);
}

TEST_F(CudaStreamTest, WaitStreamZeroAddrThrows) {
    cuda::CudaStream_t stream = cuda::getStream();
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
        [&]() { cuda::waitStream(stream, 0, 42); });
    cuda::releaseStream(stream);
}

TEST_F(CudaStreamTest, WriteStreamNullptrThrows) {
    constexpr size_t size = sizeof(uint32_t);
    cuda::CudaDevicePtr_t dev_ptr = cuda::alloc(size);
    EXPECT_NE(dev_ptr, 0);
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        [&]() { cuda::writeStream(nullptr, dev_ptr, 123); });
    cuda::free(dev_ptr, size);
}

TEST_F(CudaStreamTest, WriteStreamZeroAddrThrows) {
    cuda::CudaStream_t stream = cuda::getStream();
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
        [&]() { cuda::writeStream(stream, 0, 123); });
    cuda::releaseStream(stream);
}

TEST_F(CudaStreamTest, StreamLifecycle) {
    cuda::CudaStream_t stream = cuda::getStream();
    EXPECT_NE(stream, nullptr);
    cuda::synchronizeStream(stream);
    cuda::releaseStream(stream);
}

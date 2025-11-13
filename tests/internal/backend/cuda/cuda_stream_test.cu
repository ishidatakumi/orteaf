/**
 * @file cuda_stream_test.cpp
 * @brief Tests for CUDA stream creation, synchronization, and signaling helpers.
 */

#include "orteaf/internal/backend/cuda/cuda_stream.h"
#include "orteaf/internal/backend/cuda/cuda_context.h"
#include "orteaf/internal/backend/cuda/cuda_init.h"
#include "orteaf/internal/backend/cuda/cuda_device.h"
#include "orteaf/internal/backend/cuda/cuda_alloc.h"
#include "tests/internal/testing/error_assert.h"

#include <gtest/gtest.h>

namespace cuda = orteaf::internal::backend::cuda;

#if ORTEAF_ENABLE_CUDA

class CudaStreamTest : public ::testing::Test {
protected:
    void SetUp() override {
        cuda::cudaInit();
        int count = cuda::getDeviceCount();
        if (count == 0) {
            GTEST_SKIP() << "No CUDA devices available";
        }
        device_ = cuda::get_device(0);
        context_ = cuda::get_primary_context(device_);
        EXPECT_NE(context_, nullptr);
        cuda::set_context(context_);
    }

    void TearDown() override {
        if (context_ != nullptr) {
            cuda::release_primary_context(device_);
        }
    }

    cuda::CUdevice_t device_ = 0;
    cuda::CUcontext_t context_ = nullptr;
};

TEST_F(CudaStreamTest, GetStreamSucceeds) {
    cuda::CUstream_t stream = cuda::get_stream();
    EXPECT_NE(stream, nullptr);
    cuda::release_stream(stream);
}

TEST_F(CudaStreamTest, CreateMultipleStreams) {
    cuda::CUstream_t stream1 = cuda::get_stream();
    cuda::CUstream_t stream2 = cuda::get_stream();
    EXPECT_NE(stream1, nullptr);
    EXPECT_NE(stream2, nullptr);
    EXPECT_NE(stream1, stream2);
    cuda::release_stream(stream1);
    cuda::release_stream(stream2);
}

TEST_F(CudaStreamTest, ReleaseStreamSucceeds) {
    cuda::CUstream_t stream = cuda::get_stream();
    EXPECT_NO_THROW(cuda::release_stream(stream));
}

TEST_F(CudaStreamTest, ReleaseStreamNullptrNoOp) {
    EXPECT_NO_THROW(cuda::release_stream(nullptr));
}

TEST_F(CudaStreamTest, SynchronizeStreamSucceeds) {
    cuda::CUstream_t stream = cuda::get_stream();
    EXPECT_NO_THROW(cuda::synchronize_stream(stream));
    cuda::release_stream(stream);
}

TEST_F(CudaStreamTest, SynchronizeStreamNullptrThrows) {
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        []() { cuda::synchronize_stream(nullptr); });
}

TEST_F(CudaStreamTest, WaitStreamSucceeds) {
    cuda::CUstream_t stream = cuda::get_stream();
    constexpr size_t size = sizeof(uint32_t);
    cuda::CUdeviceptr_t dev_ptr = cuda::alloc(size);
    EXPECT_NE(dev_ptr, 0);
    EXPECT_NO_THROW(cuda::write_stream(stream, dev_ptr, 42));
    EXPECT_NO_THROW(cuda::wait_stream(stream, dev_ptr, 42));
    cuda::free(dev_ptr, size);
    cuda::release_stream(stream);
}

TEST_F(CudaStreamTest, WriteStreamSucceeds) {
    cuda::CUstream_t stream = cuda::get_stream();
    constexpr size_t size = sizeof(uint32_t);
    cuda::CUdeviceptr_t dev_ptr = cuda::alloc(size);
    EXPECT_NE(dev_ptr, 0);
    EXPECT_NO_THROW(cuda::write_stream(stream, dev_ptr, 123));
    cuda::free(dev_ptr, size);
    cuda::release_stream(stream);
}

TEST_F(CudaStreamTest, WaitStreamNullptrThrows) {
    constexpr size_t size = sizeof(uint32_t);
    cuda::CUdeviceptr_t dev_ptr = cuda::alloc(size);
    EXPECT_NE(dev_ptr, 0);
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        [&]() { cuda::wait_stream(nullptr, dev_ptr, 42); });
    cuda::free(dev_ptr, size);
}

TEST_F(CudaStreamTest, WaitStreamZeroAddrThrows) {
    cuda::CUstream_t stream = cuda::get_stream();
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
        [&]() { cuda::wait_stream(stream, 0, 42); });
    cuda::release_stream(stream);
}

TEST_F(CudaStreamTest, WriteStreamNullptrThrows) {
    constexpr size_t size = sizeof(uint32_t);
    cuda::CUdeviceptr_t dev_ptr = cuda::alloc(size);
    EXPECT_NE(dev_ptr, 0);
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        [&]() { cuda::write_stream(nullptr, dev_ptr, 123); });
    cuda::free(dev_ptr, size);
}

TEST_F(CudaStreamTest, WriteStreamZeroAddrThrows) {
    cuda::CUstream_t stream = cuda::get_stream();
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
        [&]() { cuda::write_stream(stream, 0, 123); });
    cuda::release_stream(stream);
}

TEST_F(CudaStreamTest, StreamLifecycle) {
    cuda::CUstream_t stream = cuda::get_stream();
    EXPECT_NE(stream, nullptr);
    cuda::synchronize_stream(stream);
    cuda::release_stream(stream);
}

#else  // !ORTEAF_ENABLE_CUDA

TEST(CudaStream, DisabledThrowsBackendUnavailable) {
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::BackendUnavailable,
        [] { cuda::get_stream(); });
    
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::BackendUnavailable,
        [] { cuda::release_stream(nullptr); });
    
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::BackendUnavailable,
        [] { cuda::synchronize_stream(nullptr); });
    
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::BackendUnavailable,
        [] { cuda::wait_stream(nullptr, 0, 0); });
    
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::BackendUnavailable,
        [] { cuda::write_stream(nullptr, 0, 0); });
}

#endif  // ORTEAF_ENABLE_CUDA

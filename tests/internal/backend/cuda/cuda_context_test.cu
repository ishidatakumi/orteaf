/**
 * @file cuda_context_test.cpp
 * @brief Tests for CUDA context acquisition, creation, and release helpers.
 */

#include "orteaf/internal/backend/cuda/cuda_context.h"
#include "orteaf/internal/backend/cuda/cuda_init.h"
#include "orteaf/internal/backend/cuda/cuda_device.h"
#include "tests/internal/testing/error_assert.h"

#include <gtest/gtest.h>

namespace cuda = orteaf::internal::backend::cuda;

#if ORTEAF_ENABLE_CUDA

class CudaContextTest : public ::testing::Test {
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

TEST_F(CudaContextTest, GetPrimaryContextSucceeds) {
    cuda::CUcontext_t ctx = cuda::get_primary_context(device_);
    EXPECT_NE(ctx, nullptr);

    cuda::CUcontext_t ctx2 = cuda::get_primary_context(device_);
    EXPECT_NE(ctx2, nullptr);

    cuda::release_primary_context(device_);
    cuda::release_primary_context(device_);
}

TEST_F(CudaContextTest, GetPrimaryContextInvalidDeviceThrows) {
    cuda::CUdevice_t invalid_device = static_cast<cuda::CUdevice_t>(-1);
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::OutOfRange,
        [&]() { cuda::get_primary_context(invalid_device); });
}

TEST_F(CudaContextTest, CreateContextSucceeds) {
    cuda::CUcontext_t new_ctx = cuda::create_context(device_);
    EXPECT_NE(new_ctx, nullptr);
    cuda::release_context(new_ctx);
}

TEST_F(CudaContextTest, CreateContextInvalidDeviceThrows) {
    cuda::CUdevice_t invalid_device = static_cast<cuda::CUdevice_t>(-1);
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::OutOfRange,
        [&]() { cuda::create_context(invalid_device); });
}

TEST_F(CudaContextTest, SetContextSucceeds) {
    EXPECT_NO_THROW(cuda::set_context(context_));
    cuda::CUcontext_t new_ctx = cuda::create_context(device_);
    EXPECT_NO_THROW(cuda::set_context(new_ctx));
    EXPECT_NO_THROW(cuda::set_context(context_));
    cuda::release_context(new_ctx);
}

TEST_F(CudaContextTest, SetContextNullptrThrows) {
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        []() { cuda::set_context(nullptr); });
}

TEST_F(CudaContextTest, ReleaseContextSucceeds) {
    cuda::CUcontext_t new_ctx = cuda::create_context(device_);
    EXPECT_NO_THROW(cuda::release_context(new_ctx));
}

TEST_F(CudaContextTest, ReleaseContextNullptrNoOp) {
    EXPECT_NO_THROW(cuda::release_context(nullptr));
}

TEST_F(CudaContextTest, ReleasePrimaryContextSucceeds) {
    cuda::CUcontext_t ctx = cuda::get_primary_context(device_);
    EXPECT_NE(ctx, nullptr);
    EXPECT_NO_THROW(cuda::release_primary_context(device_));
}

TEST_F(CudaContextTest, ContextLifecycle) {
    cuda::CUcontext_t new_ctx = cuda::create_context(device_);
    EXPECT_NE(new_ctx, nullptr);
    cuda::set_context(new_ctx);
    cuda::set_context(context_);
    cuda::release_context(new_ctx);
}

#else  // !ORTEAF_ENABLE_CUDA

TEST(CudaContext, DisabledThrowsBackendUnavailable) {
    cuda::CUdevice_t device = 0;

    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::BackendUnavailable,
        [&] { cuda::get_primary_context(device); });
    
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::BackendUnavailable,
        [&] { cuda::create_context(device); });

    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::BackendUnavailable,
        [] { cuda::set_context(nullptr); });
    
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::BackendUnavailable,
        [] { cuda::release_context(nullptr); });
    
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::BackendUnavailable,
        [&] { cuda::release_primary_context(device); });
}

#endif  // ORTEAF_ENABLE_CUDA

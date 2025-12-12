/**
 * @file cuda_context_test.cpp
 * @brief Tests for CUDA context acquisition, creation, and release helpers.
 */

#include "orteaf/internal/runtime/cuda/platform/wrapper/cuda_context.h"
#include "orteaf/internal/runtime/cuda/platform/wrapper/cuda_init.h"
#include "orteaf/internal/runtime/cuda/platform/wrapper/cuda_device.h"
#include "tests/internal/testing/error_assert.h"

#include <gtest/gtest.h>

namespace cuda = orteaf::internal::runtime::cuda::platform::wrapper;

class CudaContextTest : public ::testing::Test {
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

TEST_F(CudaContextTest, GetPrimaryContextSucceeds) {
    cuda::CudaContext_t ctx = cuda::getPrimaryContext(device_);
    EXPECT_NE(ctx, nullptr);

    cuda::CudaContext_t ctx2 = cuda::getPrimaryContext(device_);
    EXPECT_NE(ctx2, nullptr);

    cuda::releasePrimaryContext(device_);
    cuda::releasePrimaryContext(device_);
}

TEST_F(CudaContextTest, GetPrimaryContextInvalidDeviceThrows) {
    cuda::CudaDevice_t invalid_device = static_cast<cuda::CudaDevice_t>(-1);
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::OutOfRange,
        [&]() { cuda::getPrimaryContext(invalid_device); });
}

TEST_F(CudaContextTest, CreateContextSucceeds) {
    cuda::CudaContext_t new_ctx = cuda::createContext(device_);
    EXPECT_NE(new_ctx, nullptr);
    cuda::releaseContext(new_ctx);
}

TEST_F(CudaContextTest, CreateContextInvalidDeviceThrows) {
    cuda::CudaDevice_t invalid_device = static_cast<cuda::CudaDevice_t>(-1);
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::OutOfRange,
        [&]() { cuda::createContext(invalid_device); });
}

TEST_F(CudaContextTest, SetContextSucceeds) {
    EXPECT_NO_THROW(cuda::setContext(context_));
    cuda::CudaContext_t new_ctx = cuda::createContext(device_);
    EXPECT_NO_THROW(cuda::setContext(new_ctx));
    EXPECT_NO_THROW(cuda::setContext(context_));
    cuda::releaseContext(new_ctx);
}

TEST_F(CudaContextTest, SetContextNullptrThrows) {
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        []() { cuda::setContext(nullptr); });
}

TEST_F(CudaContextTest, ReleaseContextSucceeds) {
    cuda::CudaContext_t new_ctx = cuda::createContext(device_);
    EXPECT_NO_THROW(cuda::releaseContext(new_ctx));
}

TEST_F(CudaContextTest, ReleaseContextNullptrNoOp) {
    EXPECT_NO_THROW(cuda::releaseContext(nullptr));
}

TEST_F(CudaContextTest, ReleasePrimaryContextSucceeds) {
    cuda::CudaContext_t ctx = cuda::getPrimaryContext(device_);
    EXPECT_NE(ctx, nullptr);
    EXPECT_NO_THROW(cuda::releasePrimaryContext(device_));
}

TEST_F(CudaContextTest, ContextLifecycle) {
    cuda::CudaContext_t new_ctx = cuda::createContext(device_);
    EXPECT_NE(new_ctx, nullptr);
    cuda::setContext(new_ctx);
    cuda::setContext(context_);
    cuda::releaseContext(new_ctx);
}

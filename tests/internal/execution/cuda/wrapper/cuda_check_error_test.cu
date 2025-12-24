/**
 * @file cuda_check_error_test.cpp
 * @brief Tests for CUDA error checking utilities and error code mapping.
 */

#include "orteaf/internal/execution/cuda/platform/wrapper/cuda_check.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "tests/internal/testing/error_assert.h"
#include "tests/internal/testing/error_assert.h"

#include <gtest/gtest.h>

namespace cuda = orteaf::internal::execution::cuda::platform::wrapper;
namespace diag = orteaf::internal::diagnostics::error;

/**
 * @brief Test that mapRuntimeErrc maps CUDA errors correctly.
 */
TEST(CudaCheckError, MapRuntimeErrcMapsCorrectly) {
    EXPECT_EQ(cuda::mapRuntimeErrc(cudaSuccess), diag::OrteafErrc::Success);
    EXPECT_EQ(cuda::mapRuntimeErrc(cudaErrorMemoryAllocation), diag::OrteafErrc::OutOfMemory);
    EXPECT_EQ(cuda::mapRuntimeErrc(cudaErrorInvalidValue), diag::OrteafErrc::InvalidParameter);
    EXPECT_EQ(cuda::mapRuntimeErrc(cudaErrorInitializationError), diag::OrteafErrc::BackendUnavailable);
    EXPECT_EQ(cuda::mapRuntimeErrc(cudaErrorInitializationError), diag::OrteafErrc::BackendUnavailable);
    EXPECT_EQ(cuda::mapRuntimeErrc(cudaErrorUnknown), diag::OrteafErrc::Unknown);
}

/**
 * @brief Test that mapDriverErrc maps CUDA Driver errors correctly.
 */
TEST(CudaCheckError, MapDriverErrcMapsCorrectly) {
    EXPECT_EQ(cuda::mapDriverErrc(CUDA_SUCCESS), diag::OrteafErrc::Success);
    EXPECT_EQ(cuda::mapDriverErrc(CUDA_ERROR_DEINITIALIZED), diag::OrteafErrc::BackendUnavailable);
    EXPECT_EQ(cuda::mapDriverErrc(CUDA_ERROR_NOT_INITIALIZED), diag::OrteafErrc::BackendUnavailable);
    EXPECT_EQ(cuda::mapDriverErrc(CUDA_ERROR_OUT_OF_MEMORY), diag::OrteafErrc::OutOfMemory);
    EXPECT_EQ(cuda::mapDriverErrc(CUDA_ERROR_INVALID_VALUE), diag::OrteafErrc::InvalidParameter);
    EXPECT_EQ(cuda::mapDriverErrc(CUDA_ERROR_INVALID_CONTEXT), diag::OrteafErrc::InvalidState);
}

/**
 * @brief Test that cuda_check does not throw on success.
 */
TEST(CudaCheckError, CudaCheckSuccessDoesNotThrow) {
    EXPECT_NO_THROW(cuda::cudaCheck(cudaSuccess, "test_expr", "test_file", 42));
}

/**
 * @brief Test that cuda_check throws on error.
 */
TEST(CudaCheckError, CudaCheckErrorThrows) {
    ::orteaf::tests::ExpectError(
        diag::OrteafErrc::OutOfMemory,
        [] { cuda::cudaCheck(cudaErrorMemoryAllocation, "test_expr", "test_file", 42); });
}

/**
 * @brief Test that cuda_check throws with correct error code.
 */
TEST(CudaCheckError, CudaCheckThrowsCorrectErrorCode) {
    ::orteaf::tests::ExpectErrorMessage(
        diag::OrteafErrc::OutOfMemory,
        {"alloc_test", "file.cpp", "100"},
        []() { cuda::cudaCheck(cudaErrorMemoryAllocation, "alloc_test", "file.cpp", 100); });
}

/**
 * @brief Test that cuda_check_last does not throw when no error.
 */
TEST(CudaCheckError, CudaCheckLastSuccessDoesNotThrow) {
    // Clear any previous errors
    cudaGetLastError();
    EXPECT_NO_THROW(cuda::cudaCheckLast("test_file", 42));
}

/**
 * @brief Test that cuda_check_sync is a no-op when debug flag is not set.
 */
TEST(CudaCheckError, CudaCheckSyncIsNoOpWhenDebugDisabled) {
    // Without ORTEAF_DEBUG_CUDA_SYNC, this should be a no-op
    cudaStream_t stream = nullptr;
    EXPECT_NO_THROW(cuda::cudaCheckSync(stream, "test_file", 42));
}

/**
 * @brief Test that cu_driver_check does not throw on success.
 */
TEST(CudaCheckError, CuDriverCheckSuccessDoesNotThrow) {
    EXPECT_NO_THROW(cuda::cuDriverCheck(CUDA_SUCCESS, "test_expr", "test_file", 42));
}

/**
 * @brief Test that cu_driver_check throws on error.
 */
TEST(CudaCheckError, CuDriverCheckErrorThrows) {
    ::orteaf::tests::ExpectError(
        diag::OrteafErrc::OutOfMemory,
        [] { cuda::cuDriverCheck(CUDA_ERROR_OUT_OF_MEMORY, "test_expr", "test_file", 42); });
}

/**
 * @brief Test that cu_driver_check throws with correct error code.
 */
TEST(CudaCheckError, CuDriverCheckThrowsCorrectErrorCode) {
    ::orteaf::tests::ExpectErrorMessage(
        diag::OrteafErrc::InvalidParameter,
        {"driver_test", "driver.cpp", "200"},
        []() { cuda::cuDriverCheck(CUDA_ERROR_INVALID_VALUE, "driver_test", "driver.cpp", 200); });
}

/**
 * @brief Test that try_driver_call returns true on success.
 */
TEST(CudaCheckError, TryDriverCallReturnsTrueOnSuccess) {
    bool result = cuda::tryDriverCall([]() {
        // Success case
    });
    EXPECT_TRUE(result);
}

/**
 * @brief Test that try_driver_call returns false for DEINITIALIZED error.
 */
TEST(CudaCheckError, TryDriverCallReturnsFalseForDeinitialized) {
    // Simulate DEINITIALIZED error by throwing a system_error with that message
    bool result = cuda::tryDriverCall([]() {
        throw std::system_error(
            static_cast<int>(CUDA_ERROR_DEINITIALIZED),
            std::generic_category(),
            "CUDA_ERROR_DEINITIALIZED: driver is deinitialized"
        );
    });
    EXPECT_FALSE(result);
}

/**
 * @brief Test that try_driver_call re-throws non-DEINITIALIZED errors.
 */
TEST(CudaCheckError, TryDriverCallRethrowsOtherErrors) {
    ::orteaf::tests::ExpectError(
        diag::OrteafErrc::OutOfMemory,
        [] {
            cuda::tryDriverCall([]() {
                cuda::cuDriverCheck(CUDA_ERROR_OUT_OF_MEMORY, "expr", "file", 1);
            });
        });
}

/**
 * @brief Test that try_driver_call handles non-system_error exceptions.
 */
TEST(CudaCheckError, TryDriverCallRethrowsNonSystemError) {
    ::orteaf::tests::ExpectException<std::runtime_error>([] {
        cuda::tryDriverCall([]() {
            throw std::runtime_error("other error");
        });
    });
}

/**
 * @brief Test that CUDA_CHECK macro works correctly.
 */
TEST(CudaCheckError, CudaCheckMacroWorks) {
    EXPECT_NO_THROW(CUDA_CHECK(cudaSuccess));
    ::orteaf::tests::ExpectError(
        diag::OrteafErrc::OutOfMemory,
        [] { CUDA_CHECK(cudaErrorMemoryAllocation); });
}

/**
 * @brief Test that CU_CHECK macro works correctly.
 */
TEST(CudaCheckError, CuCheckMacroWorks) {
    EXPECT_NO_THROW(CU_CHECK(CUDA_SUCCESS));
    ::orteaf::tests::ExpectError(
        diag::OrteafErrc::OutOfMemory,
        [] { CU_CHECK(CUDA_ERROR_OUT_OF_MEMORY); });
}

/**
 * @brief Test that error messages contain expression information.
 */
TEST(CudaCheckError, ErrorMessagesContainExpression) {
    ::orteaf::tests::ExpectErrorMessage(
        diag::OrteafErrc::InvalidParameter,
        {"cudaErrorInvalidValue"},
        []() { CUDA_CHECK(cudaErrorInvalidValue); });
}

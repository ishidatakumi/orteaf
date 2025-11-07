/**
 * @file cuda_check_error_test.cpp
 * @brief Tests for CUDA error checking utilities and error code mapping.
 */

#include "orteaf/internal/backend/cuda/cuda_check.h"
#include "orteaf/internal/diagnostics/error/error.h"

#include <gtest/gtest.h>

namespace cuda = orteaf::internal::backend::cuda;
namespace diag = orteaf::internal::diagnostics::error;

#if ORTEAF_ENABLE_CUDA

/**
 * @brief Test that map_runtime_errc maps CUDA errors correctly.
 */
TEST(CudaCheckError, MapRuntimeErrcMapsCorrectly) {
    EXPECT_EQ(cuda::map_runtime_errc(cudaSuccess), diag::OrteafErrc::Success);
    EXPECT_EQ(cuda::map_runtime_errc(cudaErrorMemoryAllocation), diag::OrteafErrc::OutOfMemory);
    EXPECT_EQ(cuda::map_runtime_errc(cudaErrorInvalidValue), diag::OrteafErrc::InvalidParameter);
    EXPECT_EQ(cuda::map_runtime_errc(cudaErrorInitializationError), diag::OrteafErrc::BackendUnavailable);
    EXPECT_EQ(cuda::map_runtime_errc(cudaErrorInitializationError), diag::OrteafErrc::BackendUnavailable);
    EXPECT_EQ(cuda::map_runtime_errc(cudaErrorUnknown), diag::OrteafErrc::Unknown);
}

/**
 * @brief Test that map_driver_errc maps CUDA Driver errors correctly.
 */
TEST(CudaCheckError, MapDriverErrcMapsCorrectly) {
    EXPECT_EQ(cuda::map_driver_errc(CUDA_SUCCESS), diag::OrteafErrc::Success);
    EXPECT_EQ(cuda::map_driver_errc(CUDA_ERROR_DEINITIALIZED), diag::OrteafErrc::BackendUnavailable);
    EXPECT_EQ(cuda::map_driver_errc(CUDA_ERROR_NOT_INITIALIZED), diag::OrteafErrc::BackendUnavailable);
    EXPECT_EQ(cuda::map_driver_errc(CUDA_ERROR_OUT_OF_MEMORY), diag::OrteafErrc::OutOfMemory);
    EXPECT_EQ(cuda::map_driver_errc(CUDA_ERROR_INVALID_VALUE), diag::OrteafErrc::InvalidParameter);
    EXPECT_EQ(cuda::map_driver_errc(CUDA_ERROR_INVALID_CONTEXT), diag::OrteafErrc::InvalidState);
}

/**
 * @brief Test that cuda_check does not throw on success.
 */
TEST(CudaCheckError, CudaCheckSuccessDoesNotThrow) {
    EXPECT_NO_THROW(cuda::cuda_check(cudaSuccess, "test_expr", "test_file", 42));
}

/**
 * @brief Test that cuda_check throws on error.
 */
TEST(CudaCheckError, CudaCheckErrorThrows) {
    EXPECT_THROW(
        cuda::cuda_check(cudaErrorMemoryAllocation, "test_expr", "test_file", 42),
        std::system_error
    );
}

/**
 * @brief Test that cuda_check throws with correct error code.
 */
TEST(CudaCheckError, CudaCheckThrowsCorrectErrorCode) {
    try {
        cuda::cuda_check(cudaErrorMemoryAllocation, "alloc_test", "file.cpp", 100);
        FAIL() << "Expected std::system_error to be thrown";
    } catch (const std::system_error& ex) {
        EXPECT_EQ(static_cast<diag::OrteafErrc>(ex.code().value()), diag::OrteafErrc::OutOfMemory);
        std::string what = ex.what();
        EXPECT_NE(what.find("alloc_test"), std::string::npos);
        EXPECT_NE(what.find("file.cpp"), std::string::npos);
        EXPECT_NE(what.find("100"), std::string::npos);
    }
}

/**
 * @brief Test that cuda_check_last does not throw when no error.
 */
TEST(CudaCheckError, CudaCheckLastSuccessDoesNotThrow) {
    // Clear any previous errors
    cudaGetLastError();
    EXPECT_NO_THROW(cuda::cuda_check_last("test_file", 42));
}

/**
 * @brief Test that cuda_check_sync is a no-op when debug flag is not set.
 */
TEST(CudaCheckError, CudaCheckSyncIsNoOpWhenDebugDisabled) {
    // Without ORTEAF_DEBUG_CUDA_SYNC, this should be a no-op
    cudaStream_t stream = nullptr;
    EXPECT_NO_THROW(cuda::cuda_check_sync(stream, "test_file", 42));
}

/**
 * @brief Test that cu_driver_check does not throw on success.
 */
TEST(CudaCheckError, CuDriverCheckSuccessDoesNotThrow) {
    EXPECT_NO_THROW(cuda::cu_driver_check(CUDA_SUCCESS, "test_expr", "test_file", 42));
}

/**
 * @brief Test that cu_driver_check throws on error.
 */
TEST(CudaCheckError, CuDriverCheckErrorThrows) {
    EXPECT_THROW(
        cuda::cu_driver_check(CUDA_ERROR_OUT_OF_MEMORY, "test_expr", "test_file", 42),
        std::system_error
    );
}

/**
 * @brief Test that cu_driver_check throws with correct error code.
 */
TEST(CudaCheckError, CuDriverCheckThrowsCorrectErrorCode) {
    try {
        cuda::cu_driver_check(CUDA_ERROR_INVALID_VALUE, "driver_test", "driver.cpp", 200);
        FAIL() << "Expected std::system_error to be thrown";
    } catch (const std::system_error& ex) {
        EXPECT_EQ(static_cast<diag::OrteafErrc>(ex.code().value()), diag::OrteafErrc::InvalidParameter);
        std::string what = ex.what();
        EXPECT_NE(what.find("driver_test"), std::string::npos);
        EXPECT_NE(what.find("driver.cpp"), std::string::npos);
        EXPECT_NE(what.find("200"), std::string::npos);
    }
}

/**
 * @brief Test that try_driver_call returns true on success.
 */
TEST(CudaCheckError, TryDriverCallReturnsTrueOnSuccess) {
    bool result = cuda::try_driver_call([]() {
        // Success case
    });
    EXPECT_TRUE(result);
}

/**
 * @brief Test that try_driver_call returns false for DEINITIALIZED error.
 */
TEST(CudaCheckError, TryDriverCallReturnsFalseForDeinitialized) {
    // Simulate DEINITIALIZED error by throwing a system_error with that message
    bool result = cuda::try_driver_call([]() {
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
    EXPECT_THROW(
        cuda::try_driver_call([]() {
            throw std::system_error(
                static_cast<int>(CUDA_ERROR_OUT_OF_MEMORY),
                std::generic_category(),
                "CUDA_ERROR_OUT_OF_MEMORY"
            );
        }),
        std::system_error
    );
}

/**
 * @brief Test that try_driver_call handles non-system_error exceptions.
 */
TEST(CudaCheckError, TryDriverCallRethrowsNonSystemError) {
    EXPECT_THROW(
        cuda::try_driver_call([]() {
            throw std::runtime_error("other error");
        }),
        std::runtime_error
    );
}

/**
 * @brief Test that CUDA_CHECK macro works correctly.
 */
TEST(CudaCheckError, CudaCheckMacroWorks) {
    EXPECT_NO_THROW(CUDA_CHECK(cudaSuccess));
    EXPECT_THROW(CUDA_CHECK(cudaErrorMemoryAllocation), std::system_error);
}

/**
 * @brief Test that CU_CHECK macro works correctly.
 */
TEST(CudaCheckError, CuCheckMacroWorks) {
    EXPECT_NO_THROW(CU_CHECK(CUDA_SUCCESS));
    EXPECT_THROW(CU_CHECK(CUDA_ERROR_OUT_OF_MEMORY), std::system_error);
}

/**
 * @brief Test that error messages contain expression information.
 */
TEST(CudaCheckError, ErrorMessagesContainExpression) {
    try {
        CUDA_CHECK(cudaErrorInvalidValue);
        FAIL() << "Expected exception";
    } catch (const std::system_error& ex) {
        std::string what = ex.what();
        EXPECT_NE(what.find("cudaErrorInvalidValue"), std::string::npos);
    }
}

#else  // !ORTEAF_ENABLE_CUDA

/**
 * @brief Test that error checking functions are no-ops when CUDA is disabled.
 */
TEST(CudaCheckError, DisabledFunctionsAreNoOps) {
    EXPECT_NO_THROW(cuda::cuda_check(0, "expr", "file", 1));
    EXPECT_NO_THROW(cuda::cuda_check_last("file", 1));
    EXPECT_NO_THROW(cuda::cuda_check_sync(nullptr, "file", 1));
    EXPECT_NO_THROW(cuda::cu_driver_check(0, "expr", "file", 1));
    
    bool result = cuda::try_driver_call([]() {});
    EXPECT_TRUE(result);
}

/**
 * @brief Test that macros are no-ops when CUDA is disabled.
 */
TEST(CudaCheckError, DisabledMacrosAreNoOps) {
    EXPECT_NO_THROW(CUDA_CHECK(0));
    EXPECT_NO_THROW(CUDA_CHECK_LAST());
    EXPECT_NO_THROW(CUDA_CHECK_SYNC(nullptr));
    EXPECT_NO_THROW(CU_CHECK(0));
}

#endif  // ORTEAF_ENABLE_CUDA

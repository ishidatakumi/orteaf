/**
 * @file cuda_device_test.cpp
 * @brief Tests for CUDA device discovery, selection, and capability queries.
 */

#include "orteaf/internal/runtime/cuda/platform/wrapper/cuda_device.h"
#include "orteaf/internal/runtime/cuda/platform/wrapper/cuda_init.h"
#include "tests/internal/testing/error_assert.h"

#include <gtest/gtest.h>

namespace cuda = orteaf::internal::runtime::cuda::platform::wrapper;

/**
 * @brief Test fixture that initializes CUDA before device tests.
 */
class CudaDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        cuda::cudaInit();
    }
};

/**
 * @brief Test that device count query succeeds.
 */
TEST_F(CudaDeviceTest, GetDeviceCountSucceeds) {
    int count = cuda::getDeviceCount();
    EXPECT_GE(count, 0);  // At least 0 devices (could be 0 on systems without CUDA)
}

/**
 * @brief Test that we can get device handles for valid indices.
 */
TEST_F(CudaDeviceTest, GetDeviceForValidIndex) {
    int count = cuda::getDeviceCount();
    if (count > 0) {
        cuda::CudaDevice_t device = cuda::getDevice(0);
        // CUdevice is an integer type, and device 0 can legitimately be 0
        // Verify the device is valid by querying its compute capability
        EXPECT_NO_THROW(cuda::getComputeCapability(device));
    } else {
        GTEST_SKIP() << "No CUDA devices available";
    }
}

/**
 * @brief Test that getting device 0 works.
 */
TEST_F(CudaDeviceTest, GetDeviceZero) {
    int count = cuda::getDeviceCount();
    if (count > 0) {
        EXPECT_NO_THROW(cuda::getDevice(0));
    } else {
        GTEST_SKIP() << "No CUDA devices available";
    }
}

/**
 * @brief Test that getting device with invalid index throws.
 */
TEST_F(CudaDeviceTest, GetDeviceInvalidIndexThrows) {
    int count = cuda::getDeviceCount();
    if (count > 0) {
        ::orteaf::tests::ExpectError(
            ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
            [&]() { cuda::getDevice(static_cast<uint32_t>(count)); });
    } else {
        GTEST_SKIP() << "No CUDA devices available";
    }
}


/**
 * @brief Test that compute capability query works.
 */
TEST_F(CudaDeviceTest, GetComputeCapabilitySucceeds) {
    int count = cuda::getDeviceCount();
    if (count > 0) {
        cuda::CudaDevice_t device = cuda::getDevice(0);
        cuda::ComputeCapability cap = cuda::getComputeCapability(device);
        EXPECT_GE(cap.major, 0);
        EXPECT_GE(cap.minor, 0);
        // Typical compute capabilities: 3.0, 3.5, 5.0, 5.2, 6.0, 6.1, 7.0, 7.5, 8.0, 8.6, 8.9, 9.0
        EXPECT_LE(cap.major, 10);  // Reasonable upper bound
        EXPECT_LE(cap.minor, 10);  // Reasonable upper bound
    } else {
        GTEST_SKIP() << "No CUDA devices available";
    }
}

/**
 * @brief Test that compute capability query with invalid device throws.
 */
TEST_F(CudaDeviceTest, GetComputeCapabilityInvalidDeviceThrows) {
    int count = cuda::getDeviceCount();
    if (count > 0) {
        // Use an invalid device handle (e.g., -1 or a value beyond valid range)
        cuda::CudaDevice_t invalid_device = static_cast<cuda::CudaDevice_t>(-1);
        ::orteaf::tests::ExpectError(
            ::orteaf::internal::diagnostics::error::OrteafErrc::OutOfRange,
            [&] { cuda::getComputeCapability(invalid_device); });
    } else {
        GTEST_SKIP() << "No CUDA devices available";
    }
}

/**
 * @brief Test that SM count calculation works correctly.
 */
TEST_F(CudaDeviceTest, GetSmCountCalculation) {
    // Test with known compute capabilities
    EXPECT_EQ(cuda::getSmCount({3, 0}), 30);
    EXPECT_EQ(cuda::getSmCount({3, 5}), 35);
    EXPECT_EQ(cuda::getSmCount({5, 0}), 50);
    EXPECT_EQ(cuda::getSmCount({7, 5}), 75);
    EXPECT_EQ(cuda::getSmCount({8, 0}), 80);
    EXPECT_EQ(cuda::getSmCount({8, 6}), 86);
    EXPECT_EQ(cuda::getSmCount({9, 0}), 90);
}

/**
 * @brief Test that SM count calculation works with real device capability.
 */
TEST_F(CudaDeviceTest, GetSmCountFromRealDevice) {
    int count = cuda::getDeviceCount();
    if (count > 0) {
        cuda::CudaDevice_t device = cuda::getDevice(0);
        cuda::ComputeCapability cap = cuda::getComputeCapability(device);
        int sm_count = cuda::getSmCount(cap);
        EXPECT_GT(sm_count, 0);
        EXPECT_EQ(sm_count, cap.major * 10 + cap.minor);
    } else {
        GTEST_SKIP() << "No CUDA devices available";
    }
}

/**
 * @brief Test that we can enumerate multiple devices if available.
 */
TEST_F(CudaDeviceTest, EnumerateMultipleDevices) {
    int count = cuda::getDeviceCount();
    if (count > 1) {
        // Get all devices
        std::vector<cuda::CudaDevice_t> devices;
        for (int i = 0; i < count; ++i) {
            cuda::CudaDevice_t device = cuda::getDevice(static_cast<uint32_t>(i));
            // CUdevice is an integer type, and device 0 can legitimately be 0
            // We verify device validity by querying compute capability below
            devices.push_back(device);
        }
        
        // Verify all devices are unique (or at least valid)
        for (size_t i = 0; i < devices.size(); ++i) {
            cuda::ComputeCapability cap = cuda::getComputeCapability(devices[i]);
            EXPECT_GE(cap.major, 0);
        }
    } else {
        GTEST_SKIP() << "Less than 2 CUDA devices available";
    }
}

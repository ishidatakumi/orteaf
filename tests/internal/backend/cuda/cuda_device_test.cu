/**
 * @file cuda_device_test.cpp
 * @brief Tests for CUDA device discovery, selection, and capability queries.
 */

#include "orteaf/internal/backend/cuda/cuda_device.h"
#include "orteaf/internal/backend/cuda/cuda_init.h"

#include <gtest/gtest.h>

namespace cuda = orteaf::internal::backend::cuda;

#ifdef ORTEAF_ENABLE_CUDA

/**
 * @brief Test fixture that initializes CUDA before device tests.
 */
class CudaDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        cuda::cuda_init();
    }
};

/**
 * @brief Test that device count query succeeds.
 */
TEST_F(CudaDeviceTest, GetDeviceCountSucceeds) {
    int count = cuda::get_device_count();
    EXPECT_GE(count, 0);  // At least 0 devices (could be 0 on systems without CUDA)
}

/**
 * @brief Test that we can get device handles for valid indices.
 */
TEST_F(CudaDeviceTest, GetDeviceForValidIndex) {
    int count = cuda::get_device_count();
    if (count > 0) {
        cuda::CUdevice_t device = cuda::get_device(0);
        // CUdevice is an integer type, and device 0 can legitimately be 0
        // Verify the device is valid by querying its compute capability
        EXPECT_NO_THROW(cuda::get_compute_capability(device));
    } else {
        GTEST_SKIP() << "No CUDA devices available";
    }
}

/**
 * @brief Test that getting device 0 works.
 */
TEST_F(CudaDeviceTest, GetDeviceZero) {
    int count = cuda::get_device_count();
    if (count > 0) {
        EXPECT_NO_THROW(cuda::get_device(0));
    } else {
        GTEST_SKIP() << "No CUDA devices available";
    }
}

/**
 * @brief Test that getting device with invalid index throws.
 */
TEST_F(CudaDeviceTest, GetDeviceInvalidIndexThrows) {
    int count = cuda::get_device_count();
    if (count > 0) {
        // Try to get a device beyond the available count
        EXPECT_THROW(cuda::get_device(static_cast<uint32_t>(count)), std::system_error);
    } else {
        GTEST_SKIP() << "No CUDA devices available";
    }
}


/**
 * @brief Test that compute capability query works.
 */
TEST_F(CudaDeviceTest, GetComputeCapabilitySucceeds) {
    int count = cuda::get_device_count();
    if (count > 0) {
        cuda::CUdevice_t device = cuda::get_device(0);
        cuda::ComputeCapability cap = cuda::get_compute_capability(device);
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
    int count = cuda::get_device_count();
    if (count > 0) {
        // Use an invalid device handle (e.g., -1 or a value beyond valid range)
        cuda::CUdevice_t invalid_device = static_cast<cuda::CUdevice_t>(-1);
        EXPECT_THROW(cuda::get_compute_capability(invalid_device), std::system_error);
    } else {
        GTEST_SKIP() << "No CUDA devices available";
    }
}

/**
 * @brief Test that SM count calculation works correctly.
 */
TEST_F(CudaDeviceTest, GetSmCountCalculation) {
    // Test with known compute capabilities
    EXPECT_EQ(cuda::get_sm_count({3, 0}), 30);
    EXPECT_EQ(cuda::get_sm_count({3, 5}), 35);
    EXPECT_EQ(cuda::get_sm_count({5, 0}), 50);
    EXPECT_EQ(cuda::get_sm_count({7, 5}), 75);
    EXPECT_EQ(cuda::get_sm_count({8, 0}), 80);
    EXPECT_EQ(cuda::get_sm_count({8, 6}), 86);
    EXPECT_EQ(cuda::get_sm_count({9, 0}), 90);
}

/**
 * @brief Test that SM count calculation works with real device capability.
 */
TEST_F(CudaDeviceTest, GetSmCountFromRealDevice) {
    int count = cuda::get_device_count();
    if (count > 0) {
        cuda::CUdevice_t device = cuda::get_device(0);
        cuda::ComputeCapability cap = cuda::get_compute_capability(device);
        int sm_count = cuda::get_sm_count(cap);
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
    int count = cuda::get_device_count();
    if (count > 1) {
        // Get all devices
        std::vector<cuda::CUdevice_t> devices;
        for (int i = 0; i < count; ++i) {
            cuda::CUdevice_t device = cuda::get_device(static_cast<uint32_t>(i));
            // CUdevice is an integer type, and device 0 can legitimately be 0
            // We verify device validity by querying compute capability below
            devices.push_back(device);
        }
        
        // Verify all devices are unique (or at least valid)
        for (size_t i = 0; i < devices.size(); ++i) {
            cuda::ComputeCapability cap = cuda::get_compute_capability(devices[i]);
            EXPECT_GE(cap.major, 0);
        }
    } else {
        GTEST_SKIP() << "Less than 2 CUDA devices available";
    }
}


#else  // !ORTEAF_ENABLE_CUDA

/**
 * @brief Test that device functions return neutral values when CUDA is disabled.
 */
TEST(CudaDevice, DisabledReturnsNeutralValues) {
    EXPECT_EQ(cuda::get_device_count(), 0);
    EXPECT_EQ(cuda::get_device(0), 0);
    
    cuda::CUdevice_t device = cuda::get_device(0);
    EXPECT_EQ(device, 0);
    
    cuda::ComputeCapability cap = cuda::get_compute_capability(device);
    EXPECT_EQ(cap.major, 0);
    EXPECT_EQ(cap.minor, 0);
    
    EXPECT_EQ(cuda::get_sm_count(cap), 0);
    
    // no set_device in driver-based design
}

/**
 * @brief Test that SM count calculation still works when CUDA is disabled.
 */
TEST(CudaDevice, GetSmCountWorksWhenDisabled) {
    // Should still work with the formula
    cuda::ComputeCapability cap{5, 0};
    EXPECT_EQ(cuda::get_sm_count(cap), 50);
}

#endif  // ORTEAF_ENABLE_CUDA

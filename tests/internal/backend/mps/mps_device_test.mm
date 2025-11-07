/**
 * @file mps_device_test.mm
 * @brief Tests for MPS/Metal device discovery and management.
 */

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

#include "orteaf/internal/backend/mps/mps_device.h"

#include <gtest/gtest.h>

namespace mps = orteaf::internal::backend::mps;

#ifdef ORTEAF_ENABLE_MPS

/**
 * @brief Test fixture for MPS device tests.
 */
class MpsDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // MPS doesn't require explicit initialization like CUDA
    }
};

/**
 * @brief Test that default device can be obtained.
 */
TEST_F(MpsDeviceTest, GetDeviceReturnsValidDevice) {
    mps::MPSDevice_t device = mps::get_device();
    
    if (device == nullptr) {
        GTEST_SKIP() << "No Metal devices available (unlikely on Mac)";
    }
    
    EXPECT_NE(device, nullptr);
    
    // Verify it's a valid MTLDevice by checking properties
    id<MTLDevice> objc_device = (__bridge id<MTLDevice>)device;
    EXPECT_NE(objc_device, nil);
    EXPECT_NE([objc_device name], nil);
    
    mps::device_release(device);
}

/**
 * @brief Test that device count is correct.
 */
TEST_F(MpsDeviceTest, GetDeviceCountReturnsValidCount) {
    int count = mps::get_device_count();
    EXPECT_GE(count, 0);
    
    // On Mac, there should be at least one device
    if (count == 0) {
        GTEST_SKIP() << "No Metal devices available";
    }
}

/**
 * @brief Test that get_device with index works.
 */
TEST_F(MpsDeviceTest, GetDeviceByIndexSucceeds) {
    int count = mps::get_device_count();
    if (count == 0) {
        GTEST_SKIP() << "No Metal devices available";
    }
    
    mps::MPSDevice_t device = mps::get_device(0);
    EXPECT_NE(device, nullptr);
    
    id<MTLDevice> objc_device = (__bridge id<MTLDevice>)device;
    EXPECT_NE(objc_device, nil);
    
    mps::device_release(device);
}

/**
 * @brief Test that get_device with invalid index throws NSRangeException.
 */
TEST_F(MpsDeviceTest, GetDeviceInvalidIndexReturnsNullptr) {
    int count = mps::get_device_count();
    if (count == 0) {
        GTEST_SKIP() << "No Metal devices available";
    }
    
    // Out of range - should throw NSRangeException
    @try {
        mps::MPSDevice_t device = mps::get_device(count);
        FAIL() << "Expected NSRangeException for out of range device_id";
    } @catch (NSException* exception) {
        EXPECT_TRUE([exception.name isEqualToString:NSRangeException]);
    }
    
    // Negative index - should throw NSRangeException
    @try {
        mps::MPSDevice_t device = mps::get_device(-1);
        FAIL() << "Expected NSRangeException for negative device_id";
    } @catch (NSException* exception) {
        EXPECT_TRUE([exception.name isEqualToString:NSRangeException]);
    }
}

/**
 * @brief Test that device_retain works.
 */
TEST_F(MpsDeviceTest, DeviceRetainWorks) {
    mps::MPSDevice_t device = mps::get_device();
    if (device == nullptr) {
        GTEST_SKIP() << "No Metal devices available";
    }
    
    // Retain should work without throwing
    EXPECT_NO_THROW(mps::device_retain(device));
    
    // Release twice (once for retain, once for original)
    mps::device_release(device);
    mps::device_release(device);
}

/**
 * @brief Test that device_retain with nullptr is ignored.
 */
TEST_F(MpsDeviceTest, DeviceRetainNullptrIsIgnored) {
    EXPECT_NO_THROW(mps::device_retain(nullptr));
}

/**
 * @brief Test that device_release works.
 */
TEST_F(MpsDeviceTest, DeviceReleaseWorks) {
    mps::MPSDevice_t device = mps::get_device();
    if (device == nullptr) {
        GTEST_SKIP() << "No Metal devices available";
    }
    
    EXPECT_NO_THROW(mps::device_release(device));
}

/**
 * @brief Test that device_release with nullptr is ignored.
 */
TEST_F(MpsDeviceTest, DeviceReleaseNullptrIsIgnored) {
    EXPECT_NO_THROW(mps::device_release(nullptr));
}

/**
 * @brief Test that multiple devices can be enumerated.
 */
TEST_F(MpsDeviceTest, EnumerateMultipleDevices) {
    int count = mps::get_device_count();
    if (count == 0) {
        GTEST_SKIP() << "No Metal devices available";
    }
    
    std::vector<mps::MPSDevice_t> devices;
    for (int i = 0; i < count; ++i) {
        mps::MPSDevice_t device = mps::get_device(i);
        EXPECT_NE(device, nullptr);
        devices.push_back(device);
    }
    
    // Verify all devices are valid
    for (auto device : devices) {
        id<MTLDevice> objc_device = (__bridge id<MTLDevice>)device;
        EXPECT_NE(objc_device, nil);
        mps::device_release(device);
    }
}

/**
 * @brief Test that get_device_array returns valid array.
 */
TEST_F(MpsDeviceTest, GetDeviceArrayReturnsValidArray) {
    mps::MPSDeviceArray_t array = mps::get_device_array();
    
    if (array == nullptr) {
        GTEST_SKIP() << "No Metal devices available";
    }
    
    EXPECT_NE(array, nullptr);
    
    NSArray<id<MTLDevice>>* objc_array = (__bridge NSArray<id<MTLDevice>>*)array;
    EXPECT_NE(objc_array, nil);
    EXPECT_GT([objc_array count], 0);
}

/**
 * @brief Test that device properties can be queried.
 */
TEST_F(MpsDeviceTest, DevicePropertiesAreAccessible) {
    mps::MPSDevice_t device = mps::get_device();
    if (device == nullptr) {
        GTEST_SKIP() << "No Metal devices available";
    }
    
    id<MTLDevice> objc_device = (__bridge id<MTLDevice>)device;
    
    // Check common properties
    EXPECT_NE([objc_device name], nil);
    EXPECT_NE([objc_device name].length, 0);
    EXPECT_NE([objc_device registryID], 0ULL);
    
    // Check if device supports features
    EXPECT_TRUE([objc_device supportsFamily:MTLGPUFamilyApple1] ||
                [objc_device supportsFamily:MTLGPUFamilyApple2] ||
                [objc_device supportsFamily:MTLGPUFamilyApple3] ||
                [objc_device supportsFamily:MTLGPUFamilyApple4] ||
                [objc_device supportsFamily:MTLGPUFamilyApple5] ||
                [objc_device supportsFamily:MTLGPUFamilyApple6] ||
                [objc_device supportsFamily:MTLGPUFamilyApple7] ||
                [objc_device supportsFamily:MTLGPUFamilyApple8]);
    
    mps::device_release(device);
}

/**
 * @brief Test that default device matches first device in array.
 */
TEST_F(MpsDeviceTest, DefaultDeviceMatchesFirstInArray) {
    mps::MPSDevice_t default_device = mps::get_device();
    if (default_device == nullptr) {
        GTEST_SKIP() << "No Metal devices available";
    }
    
    mps::MPSDevice_t first_device = mps::get_device(0);
    EXPECT_NE(first_device, nullptr);
    
    id<MTLDevice> default_objc = (__bridge id<MTLDevice>)default_device;
    id<MTLDevice> first_objc = (__bridge id<MTLDevice>)first_device;
    
    // They should be the same device
    EXPECT_EQ([default_objc registryID], [first_objc registryID]);
    
    mps::device_release(default_device);
    mps::device_release(first_device);
}

#else  // !ORTEAF_ENABLE_MPS

/**
 * @brief Test that device functions return nullptr when MPS is disabled.
 */
TEST(MpsDevice, DisabledReturnsNeutralValues) {
    EXPECT_EQ(mps::get_device(), nullptr);
    EXPECT_EQ(mps::get_device(0), nullptr);
    EXPECT_EQ(mps::get_device_count(), 0);
    EXPECT_EQ(mps::get_device_array(), nullptr);
    
    EXPECT_NO_THROW(mps::device_retain(nullptr));
    EXPECT_NO_THROW(mps::device_release(nullptr));
}

#endif  // ORTEAF_ENABLE_MPS

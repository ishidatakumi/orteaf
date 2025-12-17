/**
 * @file mps_heap_test.mm
 * @brief Tests for MPS/Metal heap descriptors and heaps.
 */

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include "orteaf/internal/runtime/mps/platform/wrapper/mps_device.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_heap.h"

#include "tests/internal/testing/error_assert.h"

#include <cstddef>
#include <exception>
#include <gtest/gtest.h>

namespace mps = orteaf::internal::runtime::mps::platform::wrapper;

class MpsHeapTest : public ::testing::Test {
protected:
  void SetUp() override {
    device_ = mps::getDevice();
    if (device_ == nullptr) {
      GTEST_SKIP() << "No Metal devices available";
    }
    descriptor_ = mps::createHeapDescriptor();
    if (descriptor_ == nullptr) {
      GTEST_SKIP() << "Failed to create heap descriptor";
    }
    try {
      mps::setHeapDescriptorSize(descriptor_, kDefaultHeapSize);
      mps::setHeapDescriptorStorageMode(descriptor_,
                                        mps::kMPSStorageModePrivate);
      mps::setHeapDescriptorCPUCacheMode(descriptor_,
                                         mps::kMPSCPUCacheModeDefaultCache);
      mps::setHeapDescriptorHazardTrackingMode(
          descriptor_, mps::kMPSHazardTrackingModeTracked);
      mps::setHeapDescriptorType(descriptor_, mps::kMPSHeapTypeAutomatic);
    } catch (const std::exception &ex) {
      GTEST_SKIP() << "Failed to configure heap descriptor: " << ex.what();
    }
  }

  void TearDown() override {
    if (heap_ != nullptr) {
      mps::destroyHeap(heap_);
      heap_ = nullptr;
    }
    if (descriptor_ != nullptr) {
      mps::destroyHeapDescriptor(descriptor_);
      descriptor_ = nullptr;
    }
    if (device_ != nullptr) {
      mps::deviceRelease(device_);
      device_ = nullptr;
    }
  }

  mps::MpsHeap_t createDefaultHeap() {
    if (heap_ != nullptr) {
      mps::destroyHeap(heap_);
      heap_ = nullptr;
    }
    heap_ = mps::createHeap(device_, descriptor_);
    return heap_;
  }

  static constexpr std::size_t kDefaultHeapSize = 1u << 20;
  mps::MpsDevice_t device_ = nullptr;
  mps::MpsHeapDescriptor_t descriptor_ = nullptr;
  mps::MpsHeap_t heap_ = nullptr;
};

TEST_F(MpsHeapTest, CreateHeapDescriptorSetsSize) {
  ASSERT_NE(descriptor_, nullptr);
  EXPECT_EQ(mps::getHeapDescriptorSize(descriptor_), kDefaultHeapSize);
}

TEST_F(MpsHeapTest, SetHeapDescriptorSizeZeroThrows) {
  ASSERT_NE(descriptor_, nullptr);
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
      [&] { mps::setHeapDescriptorSize(descriptor_, 0); });
  EXPECT_EQ(mps::getHeapDescriptorSize(descriptor_), kDefaultHeapSize);
}

TEST_F(MpsHeapTest, HeapCreationAndQueriesSucceed) {
  ASSERT_NE(descriptor_, nullptr);
  mps::MpsHeap_t heap = createDefaultHeap();
  ASSERT_NE(heap, nullptr);

  EXPECT_GE(mps::heapSize(heap), kDefaultHeapSize);
  EXPECT_EQ(mps::heapUsedSize(heap), 0u);
  EXPECT_EQ(mps::heapStorageMode(heap), mps::kMPSStorageModePrivate);
  EXPECT_EQ(mps::heapCPUCacheMode(heap), mps::kMPSCPUCacheModeDefaultCache);
  EXPECT_EQ(mps::heapHazardTrackingMode(heap),
            mps::kMPSHazardTrackingModeTracked);
  EXPECT_EQ(mps::heapType(heap), mps::kMPSHeapTypeAutomatic);

  EXPECT_GE(mps::heapMaxAvailableSize(heap, 256), 256u);
}

TEST_F(MpsHeapTest, HeapMaxAvailableSizeRejectsZeroAlignment) {
  ASSERT_NE(descriptor_, nullptr);
  mps::MpsHeap_t heap = createDefaultHeap();
  ASSERT_NE(heap, nullptr);
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
      [&] { (void)mps::heapMaxAvailableSize(heap, 0); });
}

TEST_F(MpsHeapTest, CreateHeapNullptrDeviceThrows) {
  ASSERT_NE(descriptor_, nullptr);
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [&] { (void)mps::createHeap(nullptr, descriptor_); });
}

TEST_F(MpsHeapTest, DestroyHeapNullptrIsIgnored) {
  EXPECT_NO_THROW(mps::destroyHeap(nullptr));
}

TEST_F(MpsHeapTest, HeapQueryFunctionsHandleNullptr) {
  EXPECT_EQ(mps::heapSize(nullptr), 0u);
  EXPECT_EQ(mps::heapUsedSize(nullptr), 0u);
  EXPECT_EQ(mps::heapResourceOptions(nullptr), mps::kMPSDefaultResourceOptions);
  EXPECT_EQ(mps::heapStorageMode(nullptr), mps::kMPSStorageModeShared);
  EXPECT_EQ(mps::heapCPUCacheMode(nullptr), mps::kMPSCPUCacheModeDefaultCache);
  EXPECT_EQ(mps::heapHazardTrackingMode(nullptr),
            mps::kMPSHazardTrackingModeDefault);
  EXPECT_EQ(mps::heapType(nullptr), mps::kMPSHeapTypeAutomatic);
}

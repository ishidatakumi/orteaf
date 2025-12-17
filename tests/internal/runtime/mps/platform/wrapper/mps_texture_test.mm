/**
 * @file mps_texture_test.mm
 * @brief Tests for MPS/Metal texture helpers.
 */

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include "orteaf/internal/runtime/mps/platform/wrapper/mps_device.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_heap.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_texture.h"

#include "tests/internal/testing/error_assert.h"

#include <array>
#include <gtest/gtest.h>
#include <system_error>
#include <vector>

namespace mps = orteaf::internal::runtime::mps::platform::wrapper;

class MpsTextureTest : public ::testing::Test {
protected:
  void SetUp() override {
    device_ = mps::getDevice();
    if (!device_) {
      GTEST_SKIP() << "Metal device unavailable";
    }
    descriptor_ = mps::createTextureDescriptor();
    ASSERT_NE(descriptor_, nullptr);
    mps::setTextureDescriptorWidth(descriptor_, kWidth);
    mps::setTextureDescriptorHeight(descriptor_, kHeight);
    mps::setTextureDescriptorPixelFormat(descriptor_, MTLPixelFormatRGBA8Unorm);
    mps::setTextureDescriptorStorageMode(descriptor_, MTLStorageModeShared);

    heap_desc_ = mps::createHeapDescriptor();
    ASSERT_NE(heap_desc_, nullptr);
    // Rough size: 4 bytes * width * height
    mps::setHeapDescriptorSize(heap_desc_, kWidth * kHeight * 4 + 4096);
    mps::setHeapDescriptorStorageMode(heap_desc_, MTLStorageModeShared);
    heap_ = mps::createHeap(device_, heap_desc_);
    ASSERT_NE(heap_, nullptr);
  }

  void TearDown() override {
    if (texture_) {
      mps::destroyTexture(texture_);
      texture_ = nullptr;
    }
    if (heap_) {
      mps::destroyHeap(heap_);
      heap_ = nullptr;
    }
    if (heap_desc_) {
      mps::destroyHeapDescriptor(heap_desc_);
      heap_desc_ = nullptr;
    }
    if (descriptor_) {
      mps::destroyTextureDescriptor(descriptor_);
      descriptor_ = nullptr;
    }
    if (device_) {
      mps::deviceRelease(device_);
      device_ = nullptr;
    }
  }

  static constexpr std::size_t kWidth = 8;
  static constexpr std::size_t kHeight = 4;

  mps::MpsDevice_t device_ = nullptr;
  mps::MpsTextureDescriptor_t descriptor_ = nullptr;
  mps::MpsHeapDescriptor_t heap_desc_ = nullptr;
  mps::MpsHeap_t heap_ = nullptr;
  mps::MpsTexture_t texture_ = nullptr;
};

TEST_F(MpsTextureTest, CreateTextureFromDevice) {
  texture_ = mps::createTexture(device_, descriptor_);
  ASSERT_NE(texture_, nullptr);
  EXPECT_EQ(mps::textureWidth(texture_), kWidth);
  EXPECT_EQ(mps::textureHeight(texture_), kHeight);
  EXPECT_EQ(mps::texturePixelFormat(texture_), MTLPixelFormatRGBA8Unorm);
}

TEST_F(MpsTextureTest, CreateTextureFromHeap) {
  texture_ = mps::createTextureFromHeap(heap_, descriptor_);
  ASSERT_NE(texture_, nullptr);
  EXPECT_EQ(mps::textureWidth(texture_), kWidth);
}

TEST_F(MpsTextureTest, ReplaceAndReadBackRegion) {
  texture_ = mps::createTexture(device_, descriptor_);
  ASSERT_NE(texture_, nullptr);

  std::vector<std::uint8_t> data(kWidth * kHeight * 4, 0);
  data[0] = 255;
  data[1] = 128;
  data[2] = 64;
  data[3] = 32;
  mps::replaceTextureRegion(texture_, data.data(), kWidth * 4,
                            kWidth * kHeight * 4, 0, 0, 0, kWidth, kHeight, 1);

  std::vector<std::uint8_t> out(data.size());
  mps::getTextureBytes(texture_, out.data(), kWidth * 4, kWidth * kHeight * 4,
                       0, 0, 0, kWidth, kHeight, 1);
  EXPECT_EQ(out[0], 255);
  EXPECT_EQ(out[1], 128);
  EXPECT_EQ(out[2], 64);
  EXPECT_EQ(out[3], 32);
}

TEST_F(MpsTextureTest, CreateTextureNullptrDeviceThrows) {
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [&] { (void)mps::createTexture(nullptr, descriptor_); });
}

TEST_F(MpsTextureTest, CreateTextureFromHeapNullptrThrows) {
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [&] { (void)mps::createTextureFromHeap(nullptr, descriptor_); });
}

TEST_F(MpsTextureTest, ReplaceRegionNullptrBytesThrows) {
  texture_ = mps::createTexture(device_, descriptor_);
  ASSERT_NE(texture_, nullptr);
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer, [&] {
        mps::replaceTextureRegion(texture_, nullptr, kWidth * 4,
                                  kWidth * kHeight * 4, 0, 0, 0, kWidth,
                                  kHeight, 1);
      });
}

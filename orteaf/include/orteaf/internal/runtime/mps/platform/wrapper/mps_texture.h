/**
 * @file mps_texture.h
 * @brief MPS/Metal texture descriptor and texture helpers.
 */
#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>

#include "orteaf/internal/runtime/mps/platform/wrapper/mps_types.h"

namespace orteaf::internal::runtime::mps::platform::wrapper {

/** Default values that mirror Metal enums. */
inline constexpr MPSTextureType_t kMPSTextureType2D = 2;
inline constexpr MPSPixelFormat_t kMPSPixelFormatRGBA8Unorm = 70; // MTLPixelFormatRGBA8Unorm

[[nodiscard]] MpsTextureDescriptor_t createTextureDescriptor();
void destroyTextureDescriptor(MpsTextureDescriptor_t descriptor);

void setTextureDescriptorType(MpsTextureDescriptor_t descriptor,
                              MPSTextureType_t type);
void setTextureDescriptorPixelFormat(MpsTextureDescriptor_t descriptor,
                                     MPSPixelFormat_t pixel_format);
void setTextureDescriptorWidth(MpsTextureDescriptor_t descriptor,
                               std::size_t width);
void setTextureDescriptorHeight(MpsTextureDescriptor_t descriptor,
                                std::size_t height);
void setTextureDescriptorDepth(MpsTextureDescriptor_t descriptor,
                               std::size_t depth);
void setTextureDescriptorArrayLength(MpsTextureDescriptor_t descriptor,
                                     std::size_t length);
void setTextureDescriptorMipmapLevelCount(MpsTextureDescriptor_t descriptor,
                                          std::size_t mip_levels);
void setTextureDescriptorStorageMode(MpsTextureDescriptor_t descriptor,
                                     MPSStorageMode_t storage_mode);
void setTextureDescriptorCPUCacheMode(MpsTextureDescriptor_t descriptor,
                                      MPSCPUCacheMode_t cache_mode);
void setTextureDescriptorHazardTrackingMode(
    MpsTextureDescriptor_t descriptor, MPSHazardTrackingMode_t hazard_mode);

[[nodiscard]] MpsTexture_t createTexture(MpsDevice_t device,
                                        MpsTextureDescriptor_t descriptor);
[[nodiscard]] MpsTexture_t createTextureFromHeap(MpsHeap_t heap,
                                                 MpsTextureDescriptor_t descriptor);

void destroyTexture(MpsTexture_t texture);

std::size_t textureWidth(MpsTexture_t texture);
std::size_t textureHeight(MpsTexture_t texture);
std::size_t textureDepth(MpsTexture_t texture);
std::size_t textureMipmapLevelCount(MpsTexture_t texture);
std::size_t textureArrayLength(MpsTexture_t texture);
MPSPixelFormat_t texturePixelFormat(MpsTexture_t texture);

void getTextureBytes(MpsTexture_t texture,
                     void *out_bytes,
                     std::size_t bytes_per_row,
                     std::size_t bytes_per_image,
                     std::size_t region_x,
                     std::size_t region_y,
                     std::size_t region_z,
                     std::size_t width,
                     std::size_t height,
                     std::size_t depth,
                     std::size_t mip_level = 0,
                     std::size_t slice = 0);

void replaceTextureRegion(MpsTexture_t texture,
                          const void *bytes,
                          std::size_t bytes_per_row,
                          std::size_t bytes_per_image,
                          std::size_t region_x,
                          std::size_t region_y,
                          std::size_t region_z,
                          std::size_t width,
                          std::size_t height,
                          std::size_t depth,
                          std::size_t mip_level = 0,
                          std::size_t slice = 0);

} // namespace orteaf::internal::runtime::mps::platform::wrapper

#endif  // ORTEAF_ENABLE_MPS

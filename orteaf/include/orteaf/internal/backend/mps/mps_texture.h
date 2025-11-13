/**
 * @file mps_texture.h
 * @brief MPS/Metal texture descriptor and texture helpers.
 */
#pragma once

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/backend/mps/mps_device.h"
#include "orteaf/internal/backend/mps/mps_heap.h"

#include <cstddef>
#include <cstdint>

namespace orteaf::internal::backend::mps {

struct MPSTextureDescriptor_st; using MPSTextureDescriptor_t = MPSTextureDescriptor_st*;
struct MPSTexture_st; using MPSTexture_t = MPSTexture_st*;

static_assert(sizeof(MPSTextureDescriptor_t) == sizeof(void*), "MPSTextureDescriptor must be pointer-sized.");
static_assert(sizeof(MPSTexture_t) == sizeof(void*), "MPSTexture must be pointer-sized.");

using MPSTextureType_t = std::uint32_t;
using MPSPixelFormat_t = std::uint32_t;
using MPSStorageMode_t = std::uint32_t;
using MPSCPUCacheMode_t = std::uint32_t;
using MPSHazardTrackingMode_t = std::uint32_t;

/** Default values that mirror Metal enums. */
inline constexpr MPSTextureType_t kMPSTextureType2D = 2;
inline constexpr MPSPixelFormat_t kMPSPixelFormatRGBA8Unorm = 70; // MTLPixelFormatRGBA8Unorm

/** Create an empty `MTLTextureDescriptor`. */
[[nodiscard]] MPSTextureDescriptor_t createTextureDescriptor();
/** Destroy a descriptor; ignores nullptr. */
void destroyTextureDescriptor(MPSTextureDescriptor_t descriptor);

/** Configure the descriptor. */
void setTextureDescriptorType(MPSTextureDescriptor_t descriptor, MPSTextureType_t type);
void setTextureDescriptorPixelFormat(MPSTextureDescriptor_t descriptor, MPSPixelFormat_t pixel_format);
void setTextureDescriptorWidth(MPSTextureDescriptor_t descriptor, std::size_t width);
void setTextureDescriptorHeight(MPSTextureDescriptor_t descriptor, std::size_t height);
void setTextureDescriptorDepth(MPSTextureDescriptor_t descriptor, std::size_t depth);
void setTextureDescriptorArrayLength(MPSTextureDescriptor_t descriptor, std::size_t length);
void setTextureDescriptorMipmapLevelCount(MPSTextureDescriptor_t descriptor, std::size_t mip_levels);
void setTextureDescriptorStorageMode(MPSTextureDescriptor_t descriptor, MPSStorageMode_t storage_mode);
void setTextureDescriptorCPUCacheMode(MPSTextureDescriptor_t descriptor, MPSCPUCacheMode_t cache_mode);
void setTextureDescriptorHazardTrackingMode(MPSTextureDescriptor_t descriptor, MPSHazardTrackingMode_t hazard_mode);

/** Create textures from device or heap. */
[[nodiscard]] MPSTexture_t createTexture(MPSDevice_t device, MPSTextureDescriptor_t descriptor);
[[nodiscard]] MPSTexture_t createTextureFromHeap(MPSHeap_t heap, MPSTextureDescriptor_t descriptor);

/** Destroy a texture; ignores nullptr. */
void destroyTexture(MPSTexture_t texture);

/** Texture information helpers. */
std::size_t textureWidth(MPSTexture_t texture);
std::size_t textureHeight(MPSTexture_t texture);
std::size_t textureDepth(MPSTexture_t texture);
std::size_t textureMipmapLevelCount(MPSTexture_t texture);
std::size_t textureArrayLength(MPSTexture_t texture);
MPSPixelFormat_t texturePixelFormat(MPSTexture_t texture);

/** CPU access helpers (host read/write). */
void getTextureBytes(MPSTexture_t texture,
                     void* out_bytes,
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

void replaceTextureRegion(MPSTexture_t texture,
                          const void* bytes,
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

} // namespace orteaf::internal::backend::mps

#endif  // ORTEAF_ENABLE_MPS
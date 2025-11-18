/**
 * @file mps_texture.mm
 * @brief Implementation of Metal texture helpers.
 */
#ifndef __OBJC__
#error "mps_texture.mm must be compiled with an Objective-C++ compiler (__OBJC__ not defined)"
#endif
#include "orteaf/internal/backend/mps/mps_texture.h"
#include "orteaf/internal/backend/mps/mps_objc_bridge.h"

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::backend::mps {

namespace {

using orteaf::internal::diagnostics::error::OrteafErrc;
using orteaf::internal::diagnostics::error::throwError;

MTLTextureDescriptor* objcDescriptor(MPSTextureDescriptor_t descriptor) {
    if (descriptor == nullptr) {
        throwError(OrteafErrc::NullPointer, "Texture descriptor cannot be nullptr");
    }
    return objcFromOpaqueNoown<MTLTextureDescriptor*>(descriptor);
}

id<MTLTexture> objcTexture(MPSTexture_t texture) {
    if (texture == nullptr) {
        throwError(OrteafErrc::NullPointer, "Texture cannot be nullptr");
    }
    return objcFromOpaqueNoown<id<MTLTexture>>(texture);
}

void validatePositive(const char* what, std::size_t value) {
    if (value == 0) {
        throwError(OrteafErrc::InvalidParameter, what);
    }
}

}  // namespace

MPSTextureDescriptor_t createTextureDescriptor() {
    auto descriptor = [MTLTextureDescriptor new];
    descriptor.textureType = static_cast<MTLTextureType>(kMPSTextureType2D);
    descriptor.pixelFormat = static_cast<MTLPixelFormat>(kMPSPixelFormatRGBA8Unorm);
    descriptor.width = 1;
    descriptor.height = 1;
    descriptor.depth = 1;
    descriptor.mipmapLevelCount = 1;
    descriptor.storageMode = MTLStorageModePrivate;
    descriptor.cpuCacheMode = MTLCPUCacheModeDefaultCache;
    descriptor.hazardTrackingMode = MTLHazardTrackingModeDefault;
    return reinterpret_cast<MPSTextureDescriptor_t>(opaqueFromObjcRetained(descriptor));
}

void destroyTextureDescriptor(MPSTextureDescriptor_t descriptor) {
    if (descriptor == nullptr) return;
    opaqueReleaseRetained(descriptor);
}

void setTextureDescriptorType(MPSTextureDescriptor_t descriptor, MPSTextureType_t type) {
    objcDescriptor(descriptor).textureType = static_cast<MTLTextureType>(type);
}

void setTextureDescriptorPixelFormat(MPSTextureDescriptor_t descriptor, MPSPixelFormat_t pixel_format) {
    objcDescriptor(descriptor).pixelFormat = static_cast<MTLPixelFormat>(pixel_format);
}

void setTextureDescriptorWidth(MPSTextureDescriptor_t descriptor, std::size_t width) {
    validatePositive("Width must be > 0", width);
    objcDescriptor(descriptor).width = width;
}

void setTextureDescriptorHeight(MPSTextureDescriptor_t descriptor, std::size_t height) {
    validatePositive("Height must be > 0", height);
    objcDescriptor(descriptor).height = height;
}

void setTextureDescriptorDepth(MPSTextureDescriptor_t descriptor, std::size_t depth) {
    validatePositive("Depth must be > 0", depth);
    objcDescriptor(descriptor).depth = depth;
}

void setTextureDescriptorArrayLength(MPSTextureDescriptor_t descriptor, std::size_t length) {
    validatePositive("Array length must be > 0", length);
    objcDescriptor(descriptor).arrayLength = length;
}

void setTextureDescriptorMipmapLevelCount(MPSTextureDescriptor_t descriptor, std::size_t mip_levels) {
    validatePositive("Mipmap level count must be > 0", mip_levels);
    objcDescriptor(descriptor).mipmapLevelCount = mip_levels;
}

void setTextureDescriptorStorageMode(MPSTextureDescriptor_t descriptor, MPSStorageMode_t storage_mode) {
    objcDescriptor(descriptor).storageMode = static_cast<MTLStorageMode>(storage_mode);
}

void setTextureDescriptorCPUCacheMode(MPSTextureDescriptor_t descriptor, MPSCPUCacheMode_t cache_mode) {
    objcDescriptor(descriptor).cpuCacheMode = static_cast<MTLCPUCacheMode>(cache_mode);
}

void setTextureDescriptorHazardTrackingMode(MPSTextureDescriptor_t descriptor, MPSHazardTrackingMode_t hazard_mode) {
    objcDescriptor(descriptor).hazardTrackingMode = static_cast<MTLHazardTrackingMode>(hazard_mode);
}

MPSTexture_t createTexture(MPSDevice_t device, MPSTextureDescriptor_t descriptor) {
    if (device == nullptr) {
        throwError(OrteafErrc::NullPointer, "createTexture: device cannot be nullptr");
    }
    id<MTLDevice> objc_device = objcFromOpaqueNoown<id<MTLDevice>>(device);
    id<MTLTexture> texture = [objc_device newTextureWithDescriptor:objcDescriptor(descriptor)];
    if (texture == nil) {
        throwError(OrteafErrc::OperationFailed, "createTexture: Metal returned null texture");
    }
    return reinterpret_cast<MPSTexture_t>(opaqueFromObjcRetained(texture));
}

MPSTexture_t createTextureFromHeap(MPSHeap_t heap, MPSTextureDescriptor_t descriptor) {
    if (heap == nullptr) {
        throwError(OrteafErrc::NullPointer, "createTextureFromHeap: heap cannot be nullptr");
    }
    id<MTLHeap> objc_heap = objcFromOpaqueNoown<id<MTLHeap>>(heap);
    id<MTLTexture> texture = [objc_heap newTextureWithDescriptor:objcDescriptor(descriptor)];
    if (texture == nil) {
        throwError(OrteafErrc::OperationFailed, "createTextureFromHeap: Metal returned null texture");
    }
    return reinterpret_cast<MPSTexture_t>(opaqueFromObjcRetained(texture));
}

void destroyTexture(MPSTexture_t texture) {
    if (texture == nullptr) return;
    opaqueReleaseRetained(texture);
}

std::size_t textureWidth(MPSTexture_t texture) {
    return objcTexture(texture).width;
}

std::size_t textureHeight(MPSTexture_t texture) {
    return objcTexture(texture).height;
}

std::size_t textureDepth(MPSTexture_t texture) {
    return objcTexture(texture).depth;
}

std::size_t textureMipmapLevelCount(MPSTexture_t texture) {
    return objcTexture(texture).mipmapLevelCount;
}

std::size_t textureArrayLength(MPSTexture_t texture) {
    return objcTexture(texture).arrayLength;
}

MPSPixelFormat_t texturePixelFormat(MPSTexture_t texture) {
    return static_cast<MPSPixelFormat_t>(objcTexture(texture).pixelFormat);
}

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
                     std::size_t mip_level,
                     std::size_t slice) {
    if (out_bytes == nullptr) {
        throwError(OrteafErrc::NullPointer, "getTextureBytes: out_bytes cannot be nullptr");
    }
    MTLRegion region = MTLRegionMake3D(region_x, region_y, region_z, width, height, depth);
    [objcTexture(texture) getBytes:out_bytes
                        bytesPerRow:bytes_per_row
                      bytesPerImage:bytes_per_image
                         fromRegion:region
                        mipmapLevel:mip_level
                              slice:slice];
}

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
                          std::size_t mip_level,
                          std::size_t slice) {
    if (bytes == nullptr) {
        throwError(OrteafErrc::NullPointer, "replaceTextureRegion: bytes cannot be nullptr");
    }
    MTLRegion region = MTLRegionMake3D(region_x, region_y, region_z, width, height, depth);
    [objcTexture(texture) replaceRegion:region
                             mipmapLevel:mip_level
                                   slice:slice
                               withBytes:bytes
                             bytesPerRow:bytes_per_row
                           bytesPerImage:bytes_per_image];
}

} // namespace orteaf::internal::backend::mps

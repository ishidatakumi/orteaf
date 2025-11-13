/**
 * @file mps_texture.mm
 * @brief Implementation of Metal texture helpers.
 */
#ifndef __OBJC__
#error "mps_texture.mm must be compiled with an Objective-C++ compiler (__OBJC__ not defined)"
#endif
#include "orteaf/internal/backend/mps/mps_texture.h"
#include "orteaf/internal/backend/mps/mps_objc_bridge.h"

#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include "orteaf/internal/diagnostics/error/error.h"
#else
#include "orteaf/internal/diagnostics/error/error_impl.h"
#endif

namespace orteaf::internal::backend::mps {

namespace {

#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
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
#endif

}  // namespace

MPSTextureDescriptor_t createTextureDescriptor() {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
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
#else
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "createTextureDescriptor: MPS backend is not available (MPS disabled)");
#endif
}

void destroyTextureDescriptor(MPSTextureDescriptor_t descriptor) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (descriptor == nullptr) return;
    opaqueReleaseRetained(descriptor);
#else
    (void)descriptor;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "destroyTextureDescriptor: MPS backend is not available (MPS disabled)");
#endif
}

void setTextureDescriptorType(MPSTextureDescriptor_t descriptor, MPSTextureType_t type) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    objcDescriptor(descriptor).textureType = static_cast<MTLTextureType>(type);
#else
    (void)descriptor; (void)type;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "setTextureDescriptorType: MPS backend is not available (MPS disabled)");
#endif
}

void setTextureDescriptorPixelFormat(MPSTextureDescriptor_t descriptor, MPSPixelFormat_t pixel_format) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    objcDescriptor(descriptor).pixelFormat = static_cast<MTLPixelFormat>(pixel_format);
#else
    (void)descriptor; (void)pixel_format;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "setTextureDescriptorPixelFormat: MPS backend is not available (MPS disabled)");
#endif
}

void setTextureDescriptorWidth(MPSTextureDescriptor_t descriptor, std::size_t width) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    validatePositive("Width must be > 0", width);
    objcDescriptor(descriptor).width = width;
#else
    (void)descriptor; (void)width;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "setTextureDescriptorWidth: MPS backend is not available (MPS disabled)");
#endif
}

void setTextureDescriptorHeight(MPSTextureDescriptor_t descriptor, std::size_t height) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    validatePositive("Height must be > 0", height);
    objcDescriptor(descriptor).height = height;
#else
    (void)descriptor; (void)height;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "setTextureDescriptorHeight: MPS backend is not available (MPS disabled)");
#endif
}

void setTextureDescriptorDepth(MPSTextureDescriptor_t descriptor, std::size_t depth) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    validatePositive("Depth must be > 0", depth);
    objcDescriptor(descriptor).depth = depth;
#else
    (void)descriptor; (void)depth;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "setTextureDescriptorDepth: MPS backend is not available (MPS disabled)");
#endif
}

void setTextureDescriptorArrayLength(MPSTextureDescriptor_t descriptor, std::size_t length) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    validatePositive("Array length must be > 0", length);
    objcDescriptor(descriptor).arrayLength = length;
#else
    (void)descriptor; (void)length;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "setTextureDescriptorArrayLength: MPS backend is not available (MPS disabled)");
#endif
}

void setTextureDescriptorMipmapLevelCount(MPSTextureDescriptor_t descriptor, std::size_t mip_levels) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    validatePositive("Mipmap level count must be > 0", mip_levels);
    objcDescriptor(descriptor).mipmapLevelCount = mip_levels;
#else
    (void)descriptor; (void)mip_levels;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "setTextureDescriptorMipmapLevelCount: MPS backend is not available (MPS disabled)");
#endif
}

void setTextureDescriptorStorageMode(MPSTextureDescriptor_t descriptor, MPSStorageMode_t storage_mode) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    objcDescriptor(descriptor).storageMode = static_cast<MTLStorageMode>(storage_mode);
#else
    (void)descriptor; (void)storage_mode;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "setTextureDescriptorStorageMode: MPS backend is not available (MPS disabled)");
#endif
}

void setTextureDescriptorCPUCacheMode(MPSTextureDescriptor_t descriptor, MPSCPUCacheMode_t cache_mode) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    objcDescriptor(descriptor).cpuCacheMode = static_cast<MTLCPUCacheMode>(cache_mode);
#else
    (void)descriptor; (void)cache_mode;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "setTextureDescriptorCPUCacheMode: MPS backend is not available (MPS disabled)");
#endif
}

void setTextureDescriptorHazardTrackingMode(MPSTextureDescriptor_t descriptor, MPSHazardTrackingMode_t hazard_mode) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    objcDescriptor(descriptor).hazardTrackingMode = static_cast<MTLHazardTrackingMode>(hazard_mode);
#else
    (void)descriptor; (void)hazard_mode;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "setTextureDescriptorHazardTrackingMode: MPS backend is not available (MPS disabled)");
#endif
}

MPSTexture_t createTexture(MPSDevice_t device, MPSTextureDescriptor_t descriptor) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (device == nullptr) {
        throwError(OrteafErrc::NullPointer, "createTexture: device cannot be nullptr");
    }
    id<MTLDevice> objc_device = objcFromOpaqueNoown<id<MTLDevice>>(device);
    id<MTLTexture> texture = [objc_device newTextureWithDescriptor:objcDescriptor(descriptor)];
    if (texture == nil) {
        throwError(OrteafErrc::OperationFailed, "createTexture: Metal returned null texture");
    }
    return reinterpret_cast<MPSTexture_t>(opaqueFromObjcRetained(texture));
#else
    (void)device; (void)descriptor;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "createTexture: MPS backend is not available (MPS disabled)");
#endif
}

MPSTexture_t createTextureFromHeap(MPSHeap_t heap, MPSTextureDescriptor_t descriptor) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (heap == nullptr) {
        throwError(OrteafErrc::NullPointer, "createTextureFromHeap: heap cannot be nullptr");
    }
    id<MTLHeap> objc_heap = objcFromOpaqueNoown<id<MTLHeap>>(heap);
    id<MTLTexture> texture = [objc_heap newTextureWithDescriptor:objcDescriptor(descriptor)];
    if (texture == nil) {
        throwError(OrteafErrc::OperationFailed, "createTextureFromHeap: Metal returned null texture");
    }
    return reinterpret_cast<MPSTexture_t>(opaqueFromObjcRetained(texture));
#else
    (void)heap; (void)descriptor;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "createTextureFromHeap: MPS backend is not available (MPS disabled)");
#endif
}

void destroyTexture(MPSTexture_t texture) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (texture == nullptr) return;
    opaqueReleaseRetained(texture);
#else
    (void)texture;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "destroyTexture: MPS backend is not available (MPS disabled)");
#endif
}

std::size_t textureWidth(MPSTexture_t texture) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    return objcTexture(texture).width;
#else
    (void)texture;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "textureWidth: MPS backend is not available (MPS disabled)");
#endif
}

std::size_t textureHeight(MPSTexture_t texture) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    return objcTexture(texture).height;
#else
    (void)texture;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "textureHeight: MPS backend is not available (MPS disabled)");
#endif
}

std::size_t textureDepth(MPSTexture_t texture) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    return objcTexture(texture).depth;
#else
    (void)texture;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "textureDepth: MPS backend is not available (MPS disabled)");
#endif
}

std::size_t textureMipmapLevelCount(MPSTexture_t texture) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    return objcTexture(texture).mipmapLevelCount;
#else
    (void)texture;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "textureMipmapLevelCount: MPS backend is not available (MPS disabled)");
#endif
}

std::size_t textureArrayLength(MPSTexture_t texture) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    return objcTexture(texture).arrayLength;
#else
    (void)texture;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "textureArrayLength: MPS backend is not available (MPS disabled)");
#endif
}

MPSPixelFormat_t texturePixelFormat(MPSTexture_t texture) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    return static_cast<MPSPixelFormat_t>(objcTexture(texture).pixelFormat);
#else
    (void)texture;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "texturePixelFormat: MPS backend is not available (MPS disabled)");
#endif
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
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
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
#else
    (void)texture; (void)out_bytes; (void)bytes_per_row; (void)bytes_per_image;
    (void)region_x; (void)region_y; (void)region_z; (void)width; (void)height; (void)depth;
    (void)mip_level; (void)slice;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "getTextureBytes: MPS backend is not available (MPS disabled)");
#endif
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
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
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
#else
    (void)texture; (void)bytes; (void)bytes_per_row; (void)bytes_per_image;
    (void)region_x; (void)region_y; (void)region_z; (void)width; (void)height; (void)depth;
    (void)mip_level; (void)slice;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "replaceTextureRegion: MPS backend is not available (MPS disabled)");
#endif
}

} // namespace orteaf::internal::backend::mps

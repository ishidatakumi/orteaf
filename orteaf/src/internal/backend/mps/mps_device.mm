/**
 * @file mps_device.mm
 * @brief Implementation of MPS/Metal device helpers (default/list/retain/release).
 */
#include "orteaf/internal/backend/mps/mps_device.h"
#include "orteaf/internal/backend/mps/mps_objc_bridge.h"

#include <string>
#include <string_view>

#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/diagnostics/log/log.h"
#endif

namespace orteaf::internal::backend::mps {

/**
 * @copydoc orteaf::internal::backend::mps::get_device
 */
MPSDevice_t get_device() {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (device == nil) {
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::BackendUnavailable, "get_device: no default Metal device available");
    }
    return (MPSDevice_t)opaque_from_objc_retained(device);
#else
    return nullptr;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::get_device(MPSInt_t)
 */
MPSDevice_t get_device(MPSInt_t device_id) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    NSArray<id<MTLDevice>>* devices = MTLCopyAllDevices();
    if (devices == nil) {
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::BackendUnavailable, "get_device: no Metal devices available");
    }
    ORTEAF_LOG_WARN_IF(Mps, device_id < 0, "get_device: device_id cannot be negative, returning nullptr");
    NSUInteger index = static_cast<NSUInteger>(device_id);
    ORTEAF_LOG_WARN_IF(Mps, index >= [devices count], "get_device: device_id out of range, returning nullptr");
    id<MTLDevice> device = [devices objectAtIndex:index];
    MPSDevice_t handle = (MPSDevice_t)opaque_from_objc_retained(device);
    [devices release];
    return handle;
#else
    (void)device_id;
    return nullptr;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::get_device_count
 */
int get_device_count() {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    NSArray<id<MTLDevice>>* devices = MTLCopyAllDevices();
    NSUInteger count = devices != nil ? [devices count] : 0;
    if (devices != nil) {
        [devices release];
    }
    return static_cast<int>(count);
#else
    return 0;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::device_retain
 */
void device_retain(MPSDevice_t device) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (device == nullptr) {
        return;
    }
    id<MTLDevice> objc_device = objc_from_opaque_noown<id<MTLDevice>>(device);
    [objc_device retain];
#else
    (void)device;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::device_release
 */
void device_release(MPSDevice_t device) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (device == nullptr) {
        return;
    }
    id<MTLDevice> objc_device = objc_from_opaque_noown<id<MTLDevice>>(device);
    [objc_device release];
#else
    (void)device;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::get_device_array
 */
MPSDeviceArray_t get_device_array() {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    NSArray<id<MTLDevice>>* devices = MTLCopyAllDevices();
    return (MPSDeviceArray_t)opaque_from_objc_noown(devices);
#else
    return nullptr;
#endif
}

namespace {

#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
std::string ToStdString(NSString* str) {
    if (str == nil) {
        return {};
    }
    const char* cstr = [str UTF8String];
    return cstr ? std::string(cstr) : std::string{};
}

std::string ToLowerAscii(std::string value) {
    for (char& ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return value;
}

std::string GuessFamilyFromCapabilities(id<MTLDevice> device) {
#if defined(MTLGPUFamilyApple9)
    if ([device supportsFamily:MTLGPUFamilyApple9]) {
        return "m4";
    }
#endif
#if defined(MTLGPUFamilyApple8)
    if ([device supportsFamily:MTLGPUFamilyApple8]) {
        return "m3";
    }
#endif
#if defined(MTLGPUFamilyApple7)
    if ([device supportsFamily:MTLGPUFamilyApple7]) {
        return "m2";
    }
#endif
    return {};
}

std::string GuessFamilyFromName(id<MTLDevice> device) {
    NSString* name = [device name];
    std::string lower = ToLowerAscii(ToStdString(name));
    if (lower.find("m4") != std::string::npos) {
        return "m4";
    }
    if (lower.find("m3") != std::string::npos) {
        return "m3";
    }
    if (lower.find("m2") != std::string::npos) {
        return "m2";
    }
    if (lower.find("m1") != std::string::npos) {
        return "m1";
    }
    return {};
}
#endif

} // namespace

/**
 * @copydoc orteaf::internal::backend::mps::get_device_name
 */
std::string get_device_name(MPSDevice_t device) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (device == nullptr) {
        return {};
    }
    id<MTLDevice> objc_device = objc_from_opaque_noown<id<MTLDevice>>(device);
    return ToStdString([objc_device name]);
#else
    (void)device;
    return {};
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::get_device_vendor
 */
std::string get_device_vendor(MPSDevice_t device) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    (void)device;
    return "apple";
#else
    (void)device;
    return {};
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::get_device_metal_family
 */
std::string get_device_metal_family(MPSDevice_t device) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (device == nullptr) {
        return {};
    }
    id<MTLDevice> objc_device = objc_from_opaque_noown<id<MTLDevice>>(device);
    if (auto family = GuessFamilyFromCapabilities(objc_device); !family.empty()) {
        return family;
    }
    return GuessFamilyFromName(objc_device);
#else
    (void)device;
    return {};
#endif
}

} // namespace orteaf::internal::backend::mps

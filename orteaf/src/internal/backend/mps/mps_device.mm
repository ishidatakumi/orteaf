/**
 * @file mps_device.mm
 * @brief Implementation of MPS/Metal device helpers (default/list/retain/release).
 */
#include "orteaf/internal/backend/mps/mps_device.h"
#include "orteaf/internal/backend/mps/mps_objc_bridge.h"

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

} // namespace orteaf::internal::backend::mps

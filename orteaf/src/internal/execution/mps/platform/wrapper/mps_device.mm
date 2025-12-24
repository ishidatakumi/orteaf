/**
 * @file mps_device.mm
 * @brief Implementation of MPS/Metal device helpers
 * (default/list/retain/release).
 */
#ifndef __OBJC__
#error                                                                         \
    "mps_device.mm must be compiled with an Objective-C++ compiler (__OBJC__ not defined)"
#endif
#include "orteaf/internal/execution/mps/platform/wrapper/mps_device.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_objc_bridge.h"

#include <string>
#include <string_view>

#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/diagnostics/log/log.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

namespace orteaf::internal::execution::mps::platform::wrapper {

/**
 * @copydoc orteaf::internal::backend::mps::getDevice
 */
MpsDevice_t getDevice() {
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  if (device == nil) {
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable,
               "getDevice: no default Metal device available");
  }
  return (MpsDevice_t)opaqueFromObjcRetained(device);
}

/**
 * @copydoc orteaf::internal::backend::mps::getDevice(MPSInt_t)
 */
MpsDevice_t getDevice(MPSInt_t device_id) {
  NSArray<id<MTLDevice>> *devices = MTLCopyAllDevices();
  if (devices == nil) {
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable,
               "getDevice: no Metal devices available");
  }
  if (device_id < 0) {
    [devices release];
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::InvalidParameter,
               "getDevice: device_id cannot be negative");
  }
  NSUInteger index = static_cast<NSUInteger>(device_id);
  if (index >= [devices count]) {
    [devices release];
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::InvalidParameter,
               "getDevice: device_id out of range");
  }
  id<MTLDevice> device = [devices objectAtIndex:index];
  MpsDevice_t handle = (MpsDevice_t)opaqueFromObjcRetained(device);
  [devices release];
  return handle;
}

/**
 * @copydoc orteaf::internal::backend::mps::getDeviceCount
 */
int getDeviceCount() {
  NSArray<id<MTLDevice>> *devices = MTLCopyAllDevices();
  NSUInteger count = devices != nil ? [devices count] : 0;
  if (devices != nil) {
    [devices release];
  }
  return static_cast<int>(count);
}

/**
 * @copydoc orteaf::internal::backend::mps::deviceRetain
 */
void deviceRetain(MpsDevice_t device) {
  if (device == nullptr) {
    return;
  }
  id<MTLDevice> objc_device = objcFromOpaqueNoown<id<MTLDevice>>(device);
  [objc_device retain];
}

/**
 * @copydoc orteaf::internal::backend::mps::deviceRelease
 */
void deviceRelease(MpsDevice_t device) {
  if (device == nullptr) {
    return;
  }
  id<MTLDevice> objc_device = objcFromOpaqueNoown<id<MTLDevice>>(device);
  [objc_device release];
}

/**
 * @copydoc orteaf::internal::backend::mps::getDeviceArray
 */
MpsDeviceArray_t getDeviceArray() {
  NSArray<id<MTLDevice>> *devices = MTLCopyAllDevices();
  return (MpsDeviceArray_t)opaqueFromObjcNoown(devices);
}

namespace {

std::string toStdString(NSString *str) {
  if (str == nil) {
    return {};
  }
  const char *cstr = [str UTF8String];
  return cstr ? std::string(cstr) : std::string{};
}

std::string toLowerAscii(std::string value) {
  for (char &ch : value) {
    if (ch >= 'A' && ch <= 'Z') {
      ch = static_cast<char>(ch - 'A' + 'a');
    }
  }
  return value;
}

std::string guessFamilyFromCapabilities(id<MTLDevice> device) {
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

std::string guessFamilyFromName(id<MTLDevice> device) {
  NSString *name = [device name];
  std::string lower = toLowerAscii(toStdString(name));
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

} // namespace

/**
 * @copydoc orteaf::internal::backend::mps::getDeviceName
 */
std::string getDeviceName(MpsDevice_t device) {
  if (device == nullptr) {
    return {};
  }
  id<MTLDevice> objc_device = objcFromOpaqueNoown<id<MTLDevice>>(device);
  return toStdString([objc_device name]);
}

/**
 * @copydoc orteaf::internal::backend::mps::getDeviceVendor
 */
std::string getDeviceVendor(MpsDevice_t device) {
  (void)device;
  return "apple";
}

/**
 * @copydoc orteaf::internal::backend::mps::getDeviceMetalFamily
 */
std::string getDeviceMetalFamily(MpsDevice_t device) {
  if (device == nullptr) {
    return {};
  }
  id<MTLDevice> objc_device = objcFromOpaqueNoown<id<MTLDevice>>(device);
  if (auto family = guessFamilyFromCapabilities(objc_device); !family.empty()) {
    return family;
  }
  return guessFamilyFromName(objc_device);
}

} // namespace orteaf::internal::execution::mps::platform::wrapper

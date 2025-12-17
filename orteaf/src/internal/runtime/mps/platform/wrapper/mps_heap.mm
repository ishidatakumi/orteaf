/**
 * @file mps_heap.mm
 * @brief Implementation of MPS/Metal heap helpers.
 */
#ifndef __OBJC__
#error                                                                         \
    "mps_heap.mm must be compiled with an Objective-C++ compiler (__OBJC__ not defined)"
#endif
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_heap.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_stats.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_objc_bridge.h"

namespace orteaf::internal::runtime::mps::platform::wrapper {

namespace {

using orteaf::internal::diagnostics::error::OrteafErrc;
using orteaf::internal::diagnostics::error::throwError;

constexpr const char *kDescriptorNullMsg =
    "MPS heap descriptor cannot be nullptr";
constexpr const char *kHeapNullMsg = "MPS heap cannot be nullptr";

inline void ensureDescriptor(MpsHeapDescriptor_t descriptor) {
  if (descriptor == nullptr) {
    throwError(OrteafErrc::NullPointer, kDescriptorNullMsg);
  }
}

inline void ensureHeap(MpsHeap_t heap) {
  if (heap == nullptr) {
    throwError(OrteafErrc::NullPointer, kHeapNullMsg);
  }
}

} // namespace

MpsHeapDescriptor_t createHeapDescriptor() {
  MTLHeapDescriptor *descriptor = [[MTLHeapDescriptor alloc] init];
  descriptor.storageMode = static_cast<MTLStorageMode>(kMPSStorageModePrivate);
  descriptor.cpuCacheMode =
      static_cast<MTLCPUCacheMode>(kMPSCPUCacheModeDefaultCache);
  descriptor.hazardTrackingMode =
      static_cast<MTLHazardTrackingMode>(kMPSHazardTrackingModeDefault);
  descriptor.resourceOptions =
      static_cast<MTLResourceOptions>(kMPSDefaultResourceOptions);
  descriptor.size = 0;
  descriptor.type = static_cast<MTLHeapType>(kMPSHeapTypeAutomatic);
  return (MpsHeapDescriptor_t)opaqueFromObjcRetained(descriptor);
}

void destroyHeapDescriptor(MpsHeapDescriptor_t descriptor) {
  if (descriptor == nullptr)
    return;
  opaqueReleaseRetained(descriptor);
}

void setHeapDescriptorSize(MpsHeapDescriptor_t descriptor, std::size_t size) {
  ensureDescriptor(descriptor);
  if (size == 0) {
    throwError(OrteafErrc::InvalidParameter,
               "Heap size must be greater than zero");
  }
  MTLHeapDescriptor *objc_descriptor =
      objcFromOpaqueNoown<MTLHeapDescriptor *>(descriptor);
  objc_descriptor.size = size;
}

std::size_t getHeapDescriptorSize(MpsHeapDescriptor_t descriptor) {
  if (descriptor == nullptr) {
    return 0;
  }
  MTLHeapDescriptor *objc_descriptor =
      objcFromOpaqueNoown<MTLHeapDescriptor *>(descriptor);
  return objc_descriptor ? objc_descriptor.size : 0;
}

void setHeapDescriptorStorageMode(MpsHeapDescriptor_t descriptor,
                                  MpsStorageMode_t storage_mode) {
  ensureDescriptor(descriptor);
  MTLHeapDescriptor *objc_descriptor =
      objcFromOpaqueNoown<MTLHeapDescriptor *>(descriptor);
  objc_descriptor.storageMode = static_cast<MTLStorageMode>(storage_mode);
}

void setHeapDescriptorCPUCacheMode(MpsHeapDescriptor_t descriptor,
                                   MpsCPUCacheMode_t cache_mode) {
  ensureDescriptor(descriptor);
  MTLHeapDescriptor *objc_descriptor =
      objcFromOpaqueNoown<MTLHeapDescriptor *>(descriptor);
  objc_descriptor.cpuCacheMode = static_cast<MTLCPUCacheMode>(cache_mode);
}

void setHeapDescriptorHazardTrackingMode(MpsHeapDescriptor_t descriptor,
                                         MpsHazardTrackingMode_t hazard_mode) {
  ensureDescriptor(descriptor);
  MTLHeapDescriptor *objc_descriptor =
      objcFromOpaqueNoown<MTLHeapDescriptor *>(descriptor);
  objc_descriptor.hazardTrackingMode =
      static_cast<MTLHazardTrackingMode>(hazard_mode);
}

void setHeapDescriptorType(MpsHeapDescriptor_t descriptor,
                           MpsHeapType_t heap_type) {
  ensureDescriptor(descriptor);
  MTLHeapDescriptor *objc_descriptor =
      objcFromOpaqueNoown<MTLHeapDescriptor *>(descriptor);
  objc_descriptor.type = static_cast<MTLHeapType>(heap_type);
}

void setHeapDescriptorResourceOptions(MpsHeapDescriptor_t descriptor,
                                      MpsResourceOptions_t resource_options) {
  ensureDescriptor(descriptor);
  MTLHeapDescriptor *objc_descriptor =
      objcFromOpaqueNoown<MTLHeapDescriptor *>(descriptor);
  objc_descriptor.resourceOptions =
      static_cast<MTLResourceOptions>(resource_options);
}

MpsHeap_t createHeap(MpsDevice_t device, MpsHeapDescriptor_t descriptor) {
  if (device == nullptr) {
    throwError(OrteafErrc::NullPointer, "createHeap: device cannot be nullptr");
  }
  ensureDescriptor(descriptor);
  id<MTLDevice> objc_device = objcFromOpaqueNoown<id<MTLDevice>>(device);
  MTLHeapDescriptor *objc_descriptor =
      objcFromOpaqueNoown<MTLHeapDescriptor *>(descriptor);
  id<MTLHeap> objc_heap = [objc_device newHeapWithDescriptor:objc_descriptor];
  if (objc_heap == nil) {
    throwError(OrteafErrc::OperationFailed,
               "createHeap: failed to create Metal heap");
  }
  const std::size_t bytes = [objc_heap size];
  updateAlloc(bytes);
  return (MpsHeap_t)opaqueFromObjcRetained(objc_heap);
}

void destroyHeap(MpsHeap_t heap) {
  if (heap == nullptr) {
    return;
  }
  const std::size_t bytes = heapSize(heap);
  opaqueReleaseRetained(heap);
  if (bytes > 0) {
    updateDealloc(bytes);
  }
}

std::size_t heapSize(MpsHeap_t heap) {
  if (heap == nullptr)
    return 0;
  id<MTLHeap> objc_heap = objcFromOpaqueNoown<id<MTLHeap>>(heap);
  return objc_heap ? [objc_heap size] : 0;
}

std::size_t heapUsedSize(MpsHeap_t heap) {
  if (heap == nullptr)
    return 0;
  id<MTLHeap> objc_heap = objcFromOpaqueNoown<id<MTLHeap>>(heap);
  return objc_heap ? [objc_heap usedSize] : 0;
}

std::size_t heapMaxAvailableSize(MpsHeap_t heap, std::size_t alignment) {
  ensureHeap(heap);
  if (alignment == 0) {
    throwError(OrteafErrc::InvalidParameter,
               "heapMaxAvailableSize: alignment must be > 0");
  }
  id<MTLHeap> objc_heap = objcFromOpaqueNoown<id<MTLHeap>>(heap);
  return objc_heap ? [objc_heap maxAvailableSizeWithAlignment:alignment] : 0;
}

MpsResourceOptions_t heapResourceOptions(MpsHeap_t heap) {
  if (heap == nullptr)
    return kMPSDefaultResourceOptions;
  id<MTLHeap> objc_heap = objcFromOpaqueNoown<id<MTLHeap>>(heap);
  return objc_heap
             ? static_cast<MpsResourceOptions_t>(objc_heap.resourceOptions)
             : kMPSDefaultResourceOptions;
}

MpsStorageMode_t heapStorageMode(MpsHeap_t heap) {
  if (heap == nullptr)
    return kMPSStorageModeShared;
  id<MTLHeap> objc_heap = objcFromOpaqueNoown<id<MTLHeap>>(heap);
  return objc_heap ? static_cast<MpsStorageMode_t>(objc_heap.storageMode)
                   : kMPSStorageModeShared;
}

MpsCPUCacheMode_t heapCPUCacheMode(MpsHeap_t heap) {
  if (heap == nullptr)
    return kMPSCPUCacheModeDefaultCache;
  id<MTLHeap> objc_heap = objcFromOpaqueNoown<id<MTLHeap>>(heap);
  return objc_heap ? static_cast<MpsCPUCacheMode_t>(objc_heap.cpuCacheMode)
                   : kMPSCPUCacheModeDefaultCache;
}

MpsHazardTrackingMode_t heapHazardTrackingMode(MpsHeap_t heap) {
  if (heap == nullptr)
    return kMPSHazardTrackingModeDefault;
  id<MTLHeap> objc_heap = objcFromOpaqueNoown<id<MTLHeap>>(heap);
  return objc_heap ? static_cast<MpsHazardTrackingMode_t>(
                         objc_heap.hazardTrackingMode)
                   : kMPSHazardTrackingModeDefault;
}

MpsHeapType_t heapType(MpsHeap_t heap) {
  if (heap == nullptr)
    return kMPSHeapTypeAutomatic;
  id<MTLHeap> objc_heap = objcFromOpaqueNoown<id<MTLHeap>>(heap);
  return objc_heap ? static_cast<MpsHeapType_t>(objc_heap.type)
                   : kMPSHeapTypeAutomatic;
}

} // namespace orteaf::internal::runtime::mps::platform::wrapper

/**
 * @file mps_heap.mm
 * @brief Implementation of MPS/Metal heap helpers.
 */
#ifndef __OBJC__
#error "mps_heap.mm must be compiled with an Objective-C++ compiler (__OBJC__ not defined)"
#endif
#include "orteaf/internal/backend/mps/mps_heap.h"
#include "orteaf/internal/backend/mps/mps_stats.h"

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

#include "orteaf/internal/backend/mps/mps_objc_bridge.h"
#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::backend::mps {

namespace {

using orteaf::internal::diagnostics::error::throwError;
using orteaf::internal::diagnostics::error::OrteafErrc;

constexpr const char* kDescriptorNullMsg = "MPS heap descriptor cannot be nullptr";
constexpr const char* kHeapNullMsg = "MPS heap cannot be nullptr";

inline void ensureDescriptor(MPSHeapDescriptor_t descriptor) {
    if (descriptor == nullptr) {
        throwError(OrteafErrc::NullPointer, kDescriptorNullMsg);
    }
}

inline void ensureHeap(MPSHeap_t heap) {
    if (heap == nullptr) {
        throwError(OrteafErrc::NullPointer, kHeapNullMsg);
    }
}

} // namespace

MPSHeapDescriptor_t createHeapDescriptor() {
    MTLHeapDescriptor* descriptor = [[MTLHeapDescriptor alloc] init];
    descriptor.storageMode = static_cast<MTLStorageMode>(kMPSStorageModePrivate);
    descriptor.cpuCacheMode = static_cast<MTLCPUCacheMode>(kMPSCPUCacheModeDefaultCache);
    descriptor.hazardTrackingMode = static_cast<MTLHazardTrackingMode>(kMPSHazardTrackingModeDefault);
    descriptor.resourceOptions = static_cast<MTLResourceOptions>(kMPSDefaultResourceOptions);
    descriptor.size = 0;
    descriptor.type = static_cast<MTLHeapType>(kMPSHeapTypeAutomatic);
    return (MPSHeapDescriptor_t)opaqueFromObjcRetained(descriptor);
}

void destroyHeapDescriptor(MPSHeapDescriptor_t descriptor) {
    if (descriptor == nullptr) return;
    opaqueReleaseRetained(descriptor);
}

void setHeapDescriptorSize(MPSHeapDescriptor_t descriptor, std::size_t size) {
    ensureDescriptor(descriptor);
    if (size == 0) {
        throwError(OrteafErrc::InvalidParameter, "Heap size must be greater than zero");
    }
    MTLHeapDescriptor* objc_descriptor = objcFromOpaqueNoown<MTLHeapDescriptor*>(descriptor);
    objc_descriptor.size = size;
}

std::size_t getHeapDescriptorSize(MPSHeapDescriptor_t descriptor) {
    if (descriptor == nullptr) {
        return 0;
    }
    MTLHeapDescriptor* objc_descriptor = objcFromOpaqueNoown<MTLHeapDescriptor*>(descriptor);
    return objc_descriptor ? objc_descriptor.size : 0;
}

void setHeapDescriptorStorageMode(MPSHeapDescriptor_t descriptor, MPSStorageMode_t storage_mode) {
    ensureDescriptor(descriptor);
    MTLHeapDescriptor* objc_descriptor = objcFromOpaqueNoown<MTLHeapDescriptor*>(descriptor);
    objc_descriptor.storageMode = static_cast<MTLStorageMode>(storage_mode);
}

void setHeapDescriptorCPUCacheMode(MPSHeapDescriptor_t descriptor, MPSCPUCacheMode_t cache_mode) {
    ensureDescriptor(descriptor);
    MTLHeapDescriptor* objc_descriptor = objcFromOpaqueNoown<MTLHeapDescriptor*>(descriptor);
    objc_descriptor.cpuCacheMode = static_cast<MTLCPUCacheMode>(cache_mode);
}

void setHeapDescriptorHazardTrackingMode(MPSHeapDescriptor_t descriptor, MPSHazardTrackingMode_t hazard_mode) {
    ensureDescriptor(descriptor);
    MTLHeapDescriptor* objc_descriptor = objcFromOpaqueNoown<MTLHeapDescriptor*>(descriptor);
    objc_descriptor.hazardTrackingMode = static_cast<MTLHazardTrackingMode>(hazard_mode);
}

void setHeapDescriptorType(MPSHeapDescriptor_t descriptor, MPSHeapType_t heap_type) {
    ensureDescriptor(descriptor);
    MTLHeapDescriptor* objc_descriptor = objcFromOpaqueNoown<MTLHeapDescriptor*>(descriptor);
    objc_descriptor.type = static_cast<MTLHeapType>(heap_type);
}

void setHeapDescriptorResourceOptions(MPSHeapDescriptor_t descriptor, MPSResourceOptions_t resource_options) {
    ensureDescriptor(descriptor);
    MTLHeapDescriptor* objc_descriptor = objcFromOpaqueNoown<MTLHeapDescriptor*>(descriptor);
    objc_descriptor.resourceOptions = static_cast<MTLResourceOptions>(resource_options);
}

MPSHeap_t createHeap(MPSDevice_t device, MPSHeapDescriptor_t descriptor) {
    if (device == nullptr) {
        throwError(OrteafErrc::NullPointer, "createHeap: device cannot be nullptr");
    }
    ensureDescriptor(descriptor);
    id<MTLDevice> objc_device = objcFromOpaqueNoown<id<MTLDevice>>(device);
    MTLHeapDescriptor* objc_descriptor = objcFromOpaqueNoown<MTLHeapDescriptor*>(descriptor);
    id<MTLHeap> objc_heap = [objc_device newHeapWithDescriptor:objc_descriptor];
    if (objc_heap == nil) {
        throwError(OrteafErrc::OperationFailed, "createHeap: failed to create Metal heap");
    }
    const std::size_t bytes = [objc_heap size];
    updateAlloc(bytes);
    return (MPSHeap_t)opaqueFromObjcRetained(objc_heap);
}

void destroyHeap(MPSHeap_t heap) {
    if (heap == nullptr) {
        return;
    }
    const std::size_t bytes = heapSize(heap);
    opaqueReleaseRetained(heap);
    if (bytes > 0) {
        updateDealloc(bytes);
    }
}

std::size_t heapSize(MPSHeap_t heap) {
    if (heap == nullptr) return 0;
    id<MTLHeap> objc_heap = objcFromOpaqueNoown<id<MTLHeap>>(heap);
    return objc_heap ? [objc_heap size] : 0;
}

std::size_t heapUsedSize(MPSHeap_t heap) {
    if (heap == nullptr) return 0;
    id<MTLHeap> objc_heap = objcFromOpaqueNoown<id<MTLHeap>>(heap);
    return objc_heap ? [objc_heap usedSize] : 0;
}

std::size_t heapMaxAvailableSize(MPSHeap_t heap, std::size_t alignment) {
    ensureHeap(heap);
    if (alignment == 0) {
        throwError(OrteafErrc::InvalidParameter, "heapMaxAvailableSize: alignment must be > 0");
    }
    id<MTLHeap> objc_heap = objcFromOpaqueNoown<id<MTLHeap>>(heap);
    return objc_heap ? [objc_heap maxAvailableSizeWithAlignment:alignment] : 0;
}

MPSResourceOptions_t heapResourceOptions(MPSHeap_t heap) {
    if (heap == nullptr) return kMPSDefaultResourceOptions;
    id<MTLHeap> objc_heap = objcFromOpaqueNoown<id<MTLHeap>>(heap);
    return objc_heap ? static_cast<MPSResourceOptions_t>(objc_heap.resourceOptions) : kMPSDefaultResourceOptions;
}

MPSStorageMode_t heapStorageMode(MPSHeap_t heap) {
    if (heap == nullptr) return kMPSStorageModeShared;
    id<MTLHeap> objc_heap = objcFromOpaqueNoown<id<MTLHeap>>(heap);
    return objc_heap ? static_cast<MPSStorageMode_t>(objc_heap.storageMode) : kMPSStorageModeShared;
}

MPSCPUCacheMode_t heapCPUCacheMode(MPSHeap_t heap) {
    if (heap == nullptr) return kMPSCPUCacheModeDefaultCache;
    id<MTLHeap> objc_heap = objcFromOpaqueNoown<id<MTLHeap>>(heap);
    return objc_heap ? static_cast<MPSCPUCacheMode_t>(objc_heap.cpuCacheMode) : kMPSCPUCacheModeDefaultCache;
}

MPSHazardTrackingMode_t heapHazardTrackingMode(MPSHeap_t heap) {
    if (heap == nullptr) return kMPSHazardTrackingModeDefault;
    id<MTLHeap> objc_heap = objcFromOpaqueNoown<id<MTLHeap>>(heap);
    return objc_heap ? static_cast<MPSHazardTrackingMode_t>(objc_heap.hazardTrackingMode) : kMPSHazardTrackingModeDefault;
}

MPSHeapType_t heapType(MPSHeap_t heap) {
    if (heap == nullptr) return kMPSHeapTypeAutomatic;
    id<MTLHeap> objc_heap = objcFromOpaqueNoown<id<MTLHeap>>(heap);
    return objc_heap ? static_cast<MPSHeapType_t>(objc_heap.type) : kMPSHeapTypeAutomatic;
}

} // namespace orteaf::internal::backend::mps

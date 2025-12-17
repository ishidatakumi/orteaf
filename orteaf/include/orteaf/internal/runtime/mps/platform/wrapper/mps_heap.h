/**
 * @file mps_heap.h
 * @brief MPS/Metal heap descriptor and heap helpers.
 */
#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>

#include "orteaf/internal/runtime/mps/platform/wrapper/mps_types.h"

namespace orteaf::internal::runtime::mps::platform::wrapper {

inline constexpr MpsStorageMode_t kMPSStorageModeShared = 0;
inline constexpr MpsStorageMode_t kMPSStorageModeManaged = 1;
inline constexpr MpsStorageMode_t kMPSStorageModePrivate = 2;
inline constexpr MpsStorageMode_t kMPSStorageModeMemoryless = 3;

inline constexpr MpsCPUCacheMode_t kMPSCPUCacheModeDefaultCache = 0;
inline constexpr MpsCPUCacheMode_t kMPSCPUCacheModeWriteCombined = 1;

inline constexpr MpsHazardTrackingMode_t kMPSHazardTrackingModeDefault = 0;
inline constexpr MpsHazardTrackingMode_t kMPSHazardTrackingModeTracked = 1;
inline constexpr MpsHazardTrackingMode_t kMPSHazardTrackingModeUntracked = 2;

inline constexpr MpsHeapType_t kMPSHeapTypeAutomatic = 0;
inline constexpr MpsHeapType_t kMPSHeapTypePlacement = 1;

inline constexpr MpsResourceOptions_t kMPSDefaultResourceOptions = 0;

/** Create a new `MTLHeapDescriptor` object (opaque). */
[[nodiscard]] MpsHeapDescriptor_t createHeapDescriptor();

/** Destroy a heap descriptor; ignores nullptr. */
void destroyHeapDescriptor(MpsHeapDescriptor_t descriptor);

/** Set desired heap byte size (must be > 0). */
void setHeapDescriptorSize(MpsHeapDescriptor_t descriptor, std::size_t size);

/** Get the requested heap byte size (0 when descriptor is null). */
std::size_t getHeapDescriptorSize(MpsHeapDescriptor_t descriptor);

/** Set storage mode on the descriptor. */
void setHeapDescriptorStorageMode(MpsHeapDescriptor_t descriptor,
                                  MpsStorageMode_t storage_mode);

/** Set CPU cache mode on the descriptor. */
void setHeapDescriptorCPUCacheMode(MpsHeapDescriptor_t descriptor,
                                   MpsCPUCacheMode_t cache_mode);

/** Set hazard tracking mode on the descriptor. */
void setHeapDescriptorHazardTrackingMode(MpsHeapDescriptor_t descriptor,
                                         MpsHazardTrackingMode_t hazard_mode);

/** Set heap type (automatic/placement). */
void setHeapDescriptorType(MpsHeapDescriptor_t descriptor,
                           MpsHeapType_t heap_type);

/** Set resource options bitmask on the descriptor. */
void setHeapDescriptorResourceOptions(MpsHeapDescriptor_t descriptor,
                                      MpsResourceOptions_t resource_options);

/** Create a heap from a descriptor. */
[[nodiscard]] MpsHeap_t createHeap(MpsDevice_t device,
                                   MpsHeapDescriptor_t descriptor);

/** Destroy a heap; ignores nullptr. */
void destroyHeap(MpsHeap_t heap);

/** Total bytes managed by the heap (0 if heap is null/disabled). */
std::size_t heapSize(MpsHeap_t heap);

/** Bytes currently in use within the heap. */
std::size_t heapUsedSize(MpsHeap_t heap);

/** Largest allocatable range for a given alignment (alignment must be > 0). */
std::size_t heapMaxAvailableSize(MpsHeap_t heap, std::size_t alignment);

/** Current resource options flags for the heap. */
MpsResourceOptions_t heapResourceOptions(MpsHeap_t heap);

/** The heap's storage mode. */
MpsStorageMode_t heapStorageMode(MpsHeap_t heap);

/** The heap's CPU cache mode. */
MpsCPUCacheMode_t heapCPUCacheMode(MpsHeap_t heap);

/** The heap's hazard tracking mode. */
MpsHazardTrackingMode_t heapHazardTrackingMode(MpsHeap_t heap);

/** The heap's type (automatic/placement). */
MpsHeapType_t heapType(MpsHeap_t heap);

} // namespace orteaf::internal::runtime::mps::platform::wrapper

#endif // ORTEAF_ENABLE_MPS

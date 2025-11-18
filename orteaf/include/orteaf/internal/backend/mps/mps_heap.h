/**
 * @file mps_heap.h
 * @brief MPS/Metal heap descriptor and heap helpers.
 */
#pragma once

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/backend/mps/mps_device.h"

#include <cstddef>
#include <cstdint>

namespace orteaf::internal::backend::mps {

struct MPSHeapDescriptor_st; using MPSHeapDescriptor_t = MPSHeapDescriptor_st*;
struct MPSHeap_st; using MPSHeap_t = MPSHeap_st*;

static_assert(sizeof(MPSHeapDescriptor_t) == sizeof(void*), "MPSHeapDescriptor must be pointer-sized.");
static_assert(sizeof(MPSHeap_t) == sizeof(void*), "MPSHeap must be pointer-sized.");

using MPSStorageMode_t = std::uint32_t;
using MPSCPUCacheMode_t = std::uint32_t;
using MPSHazardTrackingMode_t = std::uint32_t;
using MPSHeapType_t = std::uint32_t;
using MPSResourceOptions_t = std::uint64_t;

inline constexpr MPSStorageMode_t kMPSStorageModeShared = 0;
inline constexpr MPSStorageMode_t kMPSStorageModeManaged = 1;
inline constexpr MPSStorageMode_t kMPSStorageModePrivate = 2;
inline constexpr MPSStorageMode_t kMPSStorageModeMemoryless = 3;

inline constexpr MPSCPUCacheMode_t kMPSCPUCacheModeDefaultCache = 0;
inline constexpr MPSCPUCacheMode_t kMPSCPUCacheModeWriteCombined = 1;

inline constexpr MPSHazardTrackingMode_t kMPSHazardTrackingModeDefault = 0;
inline constexpr MPSHazardTrackingMode_t kMPSHazardTrackingModeTracked = 1;
inline constexpr MPSHazardTrackingMode_t kMPSHazardTrackingModeUntracked = 2;

inline constexpr MPSHeapType_t kMPSHeapTypeAutomatic = 0;
inline constexpr MPSHeapType_t kMPSHeapTypePlacement = 1;

inline constexpr MPSResourceOptions_t kMPSDefaultResourceOptions = 0;

/** Create a new `MTLHeapDescriptor` object (opaque). */
[[nodiscard]] MPSHeapDescriptor_t createHeapDescriptor();

/** Destroy a heap descriptor; ignores nullptr. */
void destroyHeapDescriptor(MPSHeapDescriptor_t descriptor);

/** Set desired heap byte size (must be > 0). */
void setHeapDescriptorSize(MPSHeapDescriptor_t descriptor, std::size_t size);

/** Get the requested heap byte size (0 when descriptor is null). */
std::size_t getHeapDescriptorSize(MPSHeapDescriptor_t descriptor);

/** Set storage mode on the descriptor. */
void setHeapDescriptorStorageMode(MPSHeapDescriptor_t descriptor, MPSStorageMode_t storage_mode);

/** Set CPU cache mode on the descriptor. */
void setHeapDescriptorCPUCacheMode(MPSHeapDescriptor_t descriptor, MPSCPUCacheMode_t cache_mode);

/** Set hazard tracking mode on the descriptor. */
void setHeapDescriptorHazardTrackingMode(MPSHeapDescriptor_t descriptor, MPSHazardTrackingMode_t hazard_mode);

/** Set heap type (automatic/placement). */
void setHeapDescriptorType(MPSHeapDescriptor_t descriptor, MPSHeapType_t heap_type);

/** Set resource options bitmask on the descriptor. */
void setHeapDescriptorResourceOptions(MPSHeapDescriptor_t descriptor, MPSResourceOptions_t resource_options);

/** Create a heap from a descriptor. */
[[nodiscard]] MPSHeap_t createHeap(MPSDevice_t device, MPSHeapDescriptor_t descriptor);

/** Destroy a heap; ignores nullptr. */
void destroyHeap(MPSHeap_t heap);

/** Total bytes managed by the heap (0 if heap is null/disabled). */
std::size_t heapSize(MPSHeap_t heap);

/** Bytes currently in use within the heap. */
std::size_t heapUsedSize(MPSHeap_t heap);

/** Largest allocatable range for a given alignment (alignment must be > 0). */
std::size_t heapMaxAvailableSize(MPSHeap_t heap, std::size_t alignment);

/** Current resource options flags for the heap. */
MPSResourceOptions_t heapResourceOptions(MPSHeap_t heap);

/** The heap's storage mode. */
MPSStorageMode_t heapStorageMode(MPSHeap_t heap);

/** The heap's CPU cache mode. */
MPSCPUCacheMode_t heapCPUCacheMode(MPSHeap_t heap);

/** The heap's hazard tracking mode. */
MPSHazardTrackingMode_t heapHazardTrackingMode(MPSHeap_t heap);

/** The heap's type (automatic/placement). */
MPSHeapType_t heapType(MPSHeap_t heap);

} // namespace orteaf::internal::backend::mps

#endif  // ORTEAF_ENABLE_MPS
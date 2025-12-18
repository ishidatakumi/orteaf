#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include <orteaf/internal/base/handle.h>
#include <orteaf/internal/base/lease.h>
#include <orteaf/internal/runtime/allocator/resource/mps/mps_resource.h>
#include <orteaf/internal/runtime/base/lease/control_block/raw.h>
#include <orteaf/internal/runtime/base/lease/slot.h>
#include <orteaf/internal/runtime/base/manager/base_manager_core.h>
#include <orteaf/internal/runtime/mps/manager/mps_buffer_manager.h>
#include <orteaf/internal/runtime/mps/platform/mps_slow_ops.h>
#include <orteaf/internal/runtime/mps/platform/wrapper/mps_heap.h>

namespace orteaf::internal::runtime::mps::manager {

// =============================================================================
// Heap Descriptor Key Types
// =============================================================================

struct HeapDescriptorKey {
  std::size_t size_bytes{0};
  ::orteaf::internal::runtime::mps::platform::wrapper::MpsResourceOptions_t
      resource_options{::orteaf::internal::runtime::mps::platform::wrapper::
                           kMPSDefaultResourceOptions};
  ::orteaf::internal::runtime::mps::platform::wrapper::MpsStorageMode_t
      storage_mode{::orteaf::internal::runtime::mps::platform::wrapper::
                       kMPSStorageModeShared};
  ::orteaf::internal::runtime::mps::platform::wrapper::MpsCPUCacheMode_t
      cpu_cache_mode{::orteaf::internal::runtime::mps::platform::wrapper::
                         kMPSCPUCacheModeDefaultCache};
  ::orteaf::internal::runtime::mps::platform::wrapper::MpsHazardTrackingMode_t
      hazard_tracking_mode{::orteaf::internal::runtime::mps::platform::wrapper::
                               kMPSHazardTrackingModeDefault};
  ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapType_t heap_type{
      ::orteaf::internal::runtime::mps::platform::wrapper::
          kMPSHeapTypeAutomatic};

  static HeapDescriptorKey Sized(std::size_t size) {
    HeapDescriptorKey key{};
    key.size_bytes = size;
    return key;
  }

  friend bool operator==(const HeapDescriptorKey &lhs,
                         const HeapDescriptorKey &rhs) noexcept = default;
};

struct HeapDescriptorKeyHasher {
  std::size_t operator()(const HeapDescriptorKey &key) const noexcept {
    std::size_t seed = key.size_bytes;
    constexpr std::size_t kMagic = 0x9e3779b97f4a7c15ull;
    auto mix = [&](std::size_t value) {
      seed ^= value + kMagic + (seed << 6) + (seed >> 2);
    };
    mix(static_cast<std::size_t>(key.resource_options));
    mix(static_cast<std::size_t>(key.storage_mode));
    mix(static_cast<std::size_t>(key.cpu_cache_mode));
    mix(static_cast<std::size_t>(key.hazard_tracking_mode));
    mix(static_cast<std::size_t>(key.heap_type));
    return seed;
  }
};

// =============================================================================
// Heap Resource
// =============================================================================

struct MpsHeapResource {
  ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeap_t heap{nullptr};
  std::unique_ptr<::orteaf::internal::runtime::mps::manager::MpsBufferManagerT<
      ::orteaf::internal::runtime::allocator::resource::mps::MpsResource>>
      buffer_manager;
};

// =============================================================================
// BaseManagerCore Types (RawControlBlock with initialization tracking)
// =============================================================================

using HeapSlot = ::orteaf::internal::runtime::base::RawSlot<MpsHeapResource>;
using HeapControlBlock =
    ::orteaf::internal::runtime::base::RawControlBlock<HeapSlot>;

struct MpsHeapManagerTraits {
  using ControlBlock = HeapControlBlock;
  using Handle = ::orteaf::internal::base::HeapHandle;
  static constexpr const char *Name = "MpsHeapManager";
};

// =============================================================================
// MpsHeapManager
// =============================================================================

class MpsHeapManager
    : protected ::orteaf::internal::runtime::base::BaseManagerCore<
          MpsHeapManagerTraits> {
  using Base =
      ::orteaf::internal::runtime::base::BaseManagerCore<MpsHeapManagerTraits>;

public:
  using SlowOps = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;
  using DeviceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t;
  using HeapHandle = ::orteaf::internal::base::HeapHandle;
  using HeapType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeap_t;
  using HeapLease =
      ::orteaf::internal::base::Lease<HeapHandle, HeapType, MpsHeapManager>;
  using BufferManager =
      ::orteaf::internal::runtime::mps::manager::MpsBufferManagerT<
          ::orteaf::internal::runtime::allocator::resource::mps::MpsResource>;
  using ControlBlock = typename Base::ControlBlock;

  MpsHeapManager() = default;
  MpsHeapManager(const MpsHeapManager &) = delete;
  MpsHeapManager &operator=(const MpsHeapManager &) = delete;
  MpsHeapManager(MpsHeapManager &&) = default;
  MpsHeapManager &operator=(MpsHeapManager &&) = default;
  ~MpsHeapManager() = default;

  void initialize(DeviceType device,
                  ::orteaf::internal::base::DeviceHandle device_handle,
                  MpsLibraryManager *library_manager, SlowOps *ops,
                  std::size_t capacity);
  void shutdown();

  HeapLease acquire(const HeapDescriptorKey &key);
  void release(HeapLease &lease) noexcept;

  // Direct access to BufferManager for a given heap
  BufferManager *bufferManager(const HeapLease &lease);
  BufferManager *bufferManager(const HeapDescriptorKey &key);

  // Expose base methods
  using Base::growthChunkSize;
  using Base::setGrowthChunkSize;

  // Expose base methods
  using Base::capacity;
  using Base::isAlive;
  using Base::isInitialized;

#if ORTEAF_ENABLE_TEST
  using Base::controlBlockForTest;
  using Base::freeListSizeForTest;
  using Base::isInitializedForTest;
#endif

private:
  void validateKey(const HeapDescriptorKey &key) const;
  void destroyResource(MpsHeapResource &resource);

  HeapType createHeap(const HeapDescriptorKey &key);

  std::unordered_map<HeapDescriptorKey, std::size_t, HeapDescriptorKeyHasher>
      key_to_index_{};
  DeviceType device_{nullptr};
  ::orteaf::internal::base::DeviceHandle device_handle_{};
  MpsLibraryManager *library_manager_{nullptr};
  SlowOps *ops_{nullptr};
};

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

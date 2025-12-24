#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/execution/base/lease/control_block/weak.h"
#include "orteaf/internal/execution/base/lease/weak_lease.h"
#include "orteaf/internal/execution/base/manager/base_pool_manager_core.h"
#include "orteaf/internal/execution/base/pool/fixed_slot_store.h"
#include "orteaf/internal/execution/mps/manager/mps_buffer_manager.h"
#include "orteaf/internal/execution/mps/manager/mps_library_manager.h"
#include "orteaf/internal/execution/mps/platform/mps_slow_ops.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_heap.h"

namespace orteaf::internal::execution::mps::manager {

// =============================================================================
// Heap Descriptor Key Types
// =============================================================================

struct HeapDescriptorKey {
  std::size_t size_bytes{0};
  ::orteaf::internal::execution::mps::platform::wrapper::MpsResourceOptions_t
      resource_options{::orteaf::internal::execution::mps::platform::wrapper::
                           kMPSDefaultResourceOptions};
  ::orteaf::internal::execution::mps::platform::wrapper::MpsStorageMode_t
      storage_mode{::orteaf::internal::execution::mps::platform::wrapper::
                       kMPSStorageModeShared};
  ::orteaf::internal::execution::mps::platform::wrapper::MpsCPUCacheMode_t
      cpu_cache_mode{::orteaf::internal::execution::mps::platform::wrapper::
                         kMPSCPUCacheModeDefaultCache};
  ::orteaf::internal::execution::mps::platform::wrapper::MpsHazardTrackingMode_t
      hazard_tracking_mode{::orteaf::internal::execution::mps::platform::wrapper::
                               kMPSHazardTrackingModeDefault};
  ::orteaf::internal::execution::mps::platform::wrapper::MpsHeapType_t heap_type{
      ::orteaf::internal::execution::mps::platform::wrapper::
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
// Heap Resource - MpsBufferManager を直接保持
// =============================================================================

struct MpsHeapResource {
  ::orteaf::internal::execution::mps::platform::wrapper::MpsHeap_t heap{nullptr};
  MpsBufferManagerT<
      ::orteaf::internal::execution::allocator::resource::mps::MpsResource>
      buffer_manager{};
};

// =============================================================================
// Payload Pool Traits
// =============================================================================

struct HeapPayloadPoolTraits {
  using Payload = MpsHeapResource;
  using Handle = ::orteaf::internal::base::HeapHandle;
  using DeviceType =
      ::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t;
  using SlowOps = ::orteaf::internal::execution::mps::platform::MpsSlowOps;
  using BufferManager = MpsBufferManagerT<
      ::orteaf::internal::execution::allocator::resource::mps::MpsResource>;

  struct Request {
    HeapDescriptorKey key{};
  };

  struct Context {
    DeviceType device{nullptr};
    ::orteaf::internal::base::DeviceHandle device_handle{};
    MpsLibraryManager *library_manager{nullptr};
    SlowOps *ops{nullptr};
    BufferManager::Config buffer_config{};
  };

  static bool create(Payload &payload, const Request &request,
                     const Context &context);
  static void destroy(Payload &payload, const Request &,
                      const Context &context);
};

using HeapPayloadPool = ::orteaf::internal::execution::base::pool::FixedSlotStore<
    HeapPayloadPoolTraits>;

// =============================================================================
// ControlBlock type using WeakControlBlock
// =============================================================================

using HeapControlBlock = ::orteaf::internal::execution::base::WeakControlBlock<
    ::orteaf::internal::base::HeapHandle, MpsHeapResource, HeapPayloadPool>;

// =============================================================================
// Traits for BasePoolManagerCore
// =============================================================================

struct MpsHeapManagerTraits {
  using PayloadPool = HeapPayloadPool;
  using ControlBlock = HeapControlBlock;
  struct ControlBlockTag {};
  using PayloadHandle = ::orteaf::internal::base::HeapHandle;
  static constexpr const char *Name = "MPS heap manager";
};

// =============================================================================
// MpsHeapManager
// =============================================================================

class MpsHeapManager {
  using Core = ::orteaf::internal::execution::base::BasePoolManagerCore<
      MpsHeapManagerTraits>;

public:
  using SlowOps = ::orteaf::internal::execution::mps::platform::MpsSlowOps;
  using DeviceType =
      ::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t;
  using HeapHandle = ::orteaf::internal::base::HeapHandle;
  using HeapType =
      ::orteaf::internal::execution::mps::platform::wrapper::MpsHeap_t;
  using BufferManager = MpsBufferManagerT<
      ::orteaf::internal::execution::allocator::resource::mps::MpsResource>;
  using ControlBlockHandle = Core::ControlBlockHandle;
  using ControlBlockPool = Core::ControlBlockPool;
  using HeapLease = ::orteaf::internal::execution::base::WeakLease<
      ControlBlockHandle, HeapControlBlock, ControlBlockPool, MpsHeapManager>;

  MpsHeapManager() = default;
  MpsHeapManager(const MpsHeapManager &) = delete;
  MpsHeapManager &operator=(const MpsHeapManager &) = delete;
  MpsHeapManager(MpsHeapManager &&) = default;
  MpsHeapManager &operator=(MpsHeapManager &&) = default;
  ~MpsHeapManager() = default;

  struct Config {
    DeviceType device{nullptr};
    ::orteaf::internal::base::DeviceHandle device_handle{};
    MpsLibraryManager *library_manager{nullptr};
    SlowOps *ops{nullptr};
    BufferManager::Config buffer_config{};
    std::size_t payload_capacity{0};
    std::size_t control_block_capacity{0};
    std::size_t payload_block_size{0};
    std::size_t control_block_block_size{1};
    std::size_t payload_growth_chunk_size{1};
    std::size_t control_block_growth_chunk_size{1};
  };

  void configure(const Config &config);
  void shutdown();

  HeapLease acquire(const HeapDescriptorKey &key);
  void release(HeapLease &lease) noexcept { lease.release(); }

  // Direct access to BufferManager for a given heap
  BufferManager *bufferManager(const HeapLease &lease);
  BufferManager *bufferManager(const HeapDescriptorKey &key);

#if ORTEAF_ENABLE_TEST
  bool isInitializedForTest() const noexcept { return core_.isInitialized(); }
  std::size_t payloadPoolSizeForTest() const noexcept {
    return core_.payloadPool().size();
  }
  std::size_t payloadPoolCapacityForTest() const noexcept {
    return core_.payloadPool().capacity();
  }
  std::size_t controlBlockPoolSizeForTest() const noexcept {
    return core_.controlBlockPoolSizeForTest();
  }
  std::size_t controlBlockPoolCapacityForTest() const noexcept {
    return core_.controlBlockPoolCapacityForTest();
  }
  bool isAliveForTest(HeapHandle handle) const noexcept {
    return core_.isAlive(handle);
  }
  std::size_t payloadGrowthChunkSizeForTest() const noexcept {
    return payload_growth_chunk_size_;
  }
  std::size_t controlBlockGrowthChunkSizeForTest() const noexcept {
    return core_.growthChunkSize();
  }
  bool payloadCreatedForTest(HeapHandle handle) const noexcept {
    return core_.payloadPool().isCreated(handle);
  }
  const MpsHeapResource *payloadForTest(HeapHandle handle) const noexcept {
    return core_.payloadPool().get(handle);
  }
#endif

private:
  friend HeapLease;

  void validateKey(const HeapDescriptorKey &key) const;
  HeapLease buildLease(HeapHandle handle, MpsHeapResource *payload_ptr);
  HeapPayloadPoolTraits::Context makePayloadContext() const noexcept;
  HeapType createHeap(const HeapDescriptorKey &key);

  std::unordered_map<HeapDescriptorKey, std::size_t, HeapDescriptorKeyHasher>
      key_to_index_{};
  DeviceType device_{nullptr};
  ::orteaf::internal::base::DeviceHandle device_handle_{};
  MpsLibraryManager *library_manager_{nullptr};
  SlowOps *ops_{nullptr};
  BufferManager::Config buffer_config_{};
  std::size_t payload_block_size_{0};
  std::size_t payload_growth_chunk_size_{1};
  Core core_{};
};

} // namespace orteaf::internal::execution::mps::manager

#endif // ORTEAF_ENABLE_MPS

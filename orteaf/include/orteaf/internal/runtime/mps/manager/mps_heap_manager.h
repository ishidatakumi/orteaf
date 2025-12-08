#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>

#include "orteaf/internal/runtime/mps/platform/wrapper/mps_heap.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/lease.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/runtime/mps/platform/mps_slow_ops.h"
#include "orteaf/internal/runtime/base/base_manager.h"

namespace orteaf::internal::runtime::mps::manager {

struct HeapDescriptorKey {
  std::size_t size_bytes{0};
    ::orteaf::internal::runtime::mps::platform::wrapper::MPSResourceOptions_t resource_options{
      ::orteaf::internal::runtime::mps::platform::wrapper::kMPSDefaultResourceOptions};
    ::orteaf::internal::runtime::mps::platform::wrapper::MPSStorageMode_t storage_mode{
      ::orteaf::internal::runtime::mps::platform::wrapper::kMPSStorageModeShared};
    ::orteaf::internal::runtime::mps::platform::wrapper::MPSCPUCacheMode_t cpu_cache_mode{
      ::orteaf::internal::runtime::mps::platform::wrapper::kMPSCPUCacheModeDefaultCache};
    ::orteaf::internal::runtime::mps::platform::wrapper::MPSHazardTrackingMode_t
      hazard_tracking_mode{
        ::orteaf::internal::runtime::mps::platform::wrapper::kMPSHazardTrackingModeDefault};
    ::orteaf::internal::runtime::mps::platform::wrapper::MPSHeapType_t heap_type{
      ::orteaf::internal::runtime::mps::platform::wrapper::kMPSHeapTypeAutomatic};

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

struct MpsHeapManagerState {
  HeapDescriptorKey key{};
  ::orteaf::internal::runtime::mps::platform::wrapper::MPSHeap_t heap{nullptr};
  std::uint32_t generation{0};
  bool alive{false};
  bool in_use{false};

  void reset() {
    key = HeapDescriptorKey{};
    heap = nullptr;
    alive = false;
    in_use = false;
  }
};

struct MpsHeapManagerTraits {
  using DeviceType = ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t;
  using OpsType = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;
  using StateType = MpsHeapManagerState;
  static constexpr const char *Name = "MPS heap manager";
};

class MpsHeapManager
    : public base::BaseManager<MpsHeapManager, MpsHeapManagerTraits> {
public:
  using SlowOps = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;
  using HeapLease = ::orteaf::internal::base::Lease<::orteaf::internal::base::HeapHandle,
                                                    ::orteaf::internal::runtime::mps::platform::wrapper::MPSHeap_t,
                                                    MpsHeapManager>;

  MpsHeapManager() = default;
  MpsHeapManager(const MpsHeapManager&) = delete;
  MpsHeapManager& operator=(const MpsHeapManager&) = delete;
  MpsHeapManager(MpsHeapManager&&) = default;
  MpsHeapManager& operator=(MpsHeapManager&&) = default;
  ~MpsHeapManager() = default;

  void initialize(::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t device,
                  SlowOps *slow_ops, std::size_t capacity);

  void shutdown();

  HeapLease acquire(const HeapDescriptorKey &key);

  void release(HeapLease &lease) noexcept;

#if ORTEAF_ENABLE_TEST
  struct DebugState {
    bool alive{false};
    bool heap_allocated{false};
    std::uint32_t generation{0};
    std::size_t size_bytes{0};
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSResourceOptions_t resource_options{
        ::orteaf::internal::runtime::mps::platform::wrapper::kMPSDefaultResourceOptions};
    ::orteaf::internal::runtime::mps::platform::wrapper::MPSStorageMode_t storage_mode{
      ::orteaf::internal::runtime::mps::platform::wrapper::kMPSStorageModeShared};
    ::orteaf::internal::runtime::mps::platform::wrapper::MPSCPUCacheMode_t cpu_cache_mode{
      ::orteaf::internal::runtime::mps::platform::wrapper::kMPSCPUCacheModeDefaultCache};
    ::orteaf::internal::runtime::mps::platform::wrapper::MPSHazardTrackingMode_t
      hazard_tracking_mode{
        ::orteaf::internal::runtime::mps::platform::wrapper::kMPSHazardTrackingModeDefault};
    ::orteaf::internal::runtime::mps::platform::wrapper::MPSHeapType_t heap_type{
      ::orteaf::internal::runtime::mps::platform::wrapper::kMPSHeapTypeAutomatic};
    std::size_t growth_chunk_size{0};
  };

  DebugState debugState(::orteaf::internal::base::HeapHandle handle) const;
#endif

private:
  void validateKey(const HeapDescriptorKey &key) const;

  State &ensureAliveState(::orteaf::internal::base::HeapHandle handle);

  const State &ensureAliveState(::orteaf::internal::base::HeapHandle handle) const {
    return const_cast<MpsHeapManager *>(this)->ensureAliveState(handle);
  }

  ::orteaf::internal::runtime::mps::platform::wrapper::MPSHeap_t
  createHeap(const HeapDescriptorKey &key);

  std::unordered_map<HeapDescriptorKey, std::size_t, HeapDescriptorKeyHasher>
      key_to_index_{};
};

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

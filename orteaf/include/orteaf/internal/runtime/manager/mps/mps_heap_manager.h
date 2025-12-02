#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>

#include "orteaf/internal/backend/mps/wrapper/mps_heap.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/lease.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/backend/mps/mps_slow_ops.h"

namespace orteaf::internal::runtime::mps {

struct HeapDescriptorKey {
  std::size_t size_bytes{0};
  ::orteaf::internal::backend::mps::MPSResourceOptions_t resource_options{
      ::orteaf::internal::backend::mps::kMPSDefaultResourceOptions};
  ::orteaf::internal::backend::mps::MPSStorageMode_t storage_mode{
      ::orteaf::internal::backend::mps::kMPSStorageModeShared};
  ::orteaf::internal::backend::mps::MPSCPUCacheMode_t cpu_cache_mode{
      ::orteaf::internal::backend::mps::kMPSCPUCacheModeDefaultCache};
  ::orteaf::internal::backend::mps::MPSHazardTrackingMode_t
      hazard_tracking_mode{
          ::orteaf::internal::backend::mps::kMPSHazardTrackingModeDefault};
  ::orteaf::internal::backend::mps::MPSHeapType_t heap_type{
      ::orteaf::internal::backend::mps::kMPSHeapTypeAutomatic};

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

class MpsHeapManager {
public:
  using BackendOps = ::orteaf::internal::runtime::backend_ops::mps::MpsSlowOps;
  using HeapLease = ::orteaf::internal::base::Lease<::orteaf::internal::base::HeapHandle,
                                                    ::orteaf::internal::backend::mps::MPSHeap_t,
                                                    MpsHeapManager>;

  MpsHeapManager() = default;
  MpsHeapManager(const MpsHeapManager&) = delete;
  MpsHeapManager& operator=(const MpsHeapManager&) = delete;
  MpsHeapManager(MpsHeapManager&&) = default;
  MpsHeapManager& operator=(MpsHeapManager&&) = default;
  ~MpsHeapManager() = default;

  void setGrowthChunkSize(std::size_t chunk) {
    if (chunk == 0) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          "Growth chunk size must be > 0");
    }
    growth_chunk_size_ = chunk;
  }

  std::size_t growthChunkSize() const noexcept { return growth_chunk_size_; }

  void initialize(::orteaf::internal::backend::mps::MPSDevice_t device,
                  BackendOps *ops, std::size_t capacity);

  void shutdown();

  std::size_t capacity() const noexcept { return states_.size(); }

  HeapLease getOrCreate(const HeapDescriptorKey &key);

  void release(HeapLease &lease) noexcept;

#if ORTEAF_ENABLE_TEST
  struct DebugState {
    bool alive{false};
    bool heap_allocated{false};
    std::uint32_t generation{0};
    std::size_t size_bytes{0};
    ::orteaf::internal::backend::mps::MPSResourceOptions_t resource_options{
        ::orteaf::internal::backend::mps::kMPSDefaultResourceOptions};
    ::orteaf::internal::backend::mps::MPSStorageMode_t storage_mode{
        ::orteaf::internal::backend::mps::kMPSStorageModeShared};
    ::orteaf::internal::backend::mps::MPSCPUCacheMode_t cpu_cache_mode{
        ::orteaf::internal::backend::mps::kMPSCPUCacheModeDefaultCache};
    ::orteaf::internal::backend::mps::MPSHazardTrackingMode_t
        hazard_tracking_mode{
            ::orteaf::internal::backend::mps::kMPSHazardTrackingModeDefault};
    ::orteaf::internal::backend::mps::MPSHeapType_t heap_type{
        ::orteaf::internal::backend::mps::kMPSHeapTypeAutomatic};
    std::size_t growth_chunk_size{0};
  };

  DebugState debugState(base::HeapHandle id) const;
#endif

private:
  struct State {
    HeapDescriptorKey key{};
    ::orteaf::internal::backend::mps::MPSHeap_t heap{nullptr};
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

  void ensureInitialized() const;

  void validateKey(const HeapDescriptorKey &key) const;

  State &ensureAliveState(::orteaf::internal::base::HeapHandle id);

  const State &ensureAliveState(base::HeapHandle id) const {
    return const_cast<MpsHeapManager *>(this)->ensureAliveState(id);
  }

  std::size_t allocateSlot();

  void growStatePool(std::size_t additional);

  ::orteaf::internal::backend::mps::MPSHeap_t
  createHeap(const HeapDescriptorKey &key);

  ::orteaf::internal::base::HeapVector<State> states_{};
  ::orteaf::internal::base::HeapVector<std::size_t> free_list_{};
  std::unordered_map<HeapDescriptorKey, std::size_t, HeapDescriptorKeyHasher>
      key_to_index_{};
  std::size_t growth_chunk_size_{1};
  bool initialized_{false};
  ::orteaf::internal::backend::mps::MPSDevice_t device_{nullptr};
  BackendOps *ops_{nullptr};
};

} // namespace orteaf::internal::runtime::mps

#endif // ORTEAF_ENABLE_MPS

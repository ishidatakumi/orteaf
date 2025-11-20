#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>

#include "orteaf/internal/backend/mps/mps_heap.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/base/strong_id.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/runtime/backend_ops/mps/mps_slow_ops.h"

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
                  BackendOps *ops, std::size_t capacity) {
    shutdown();
    if (device == nullptr) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          "MPS heap manager requires a valid device");
    }
    if (ops == nullptr) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          "MPS heap manager requires valid ops");
    }
    if (capacity > kMaxStateCount) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          "Requested MPS heap capacity exceeds supported limit");
    }
    device_ = device;
    ops_ = ops;
    states_.clear();
    free_list_.clear();
    key_to_index_.clear();
    states_.reserve(capacity);
    free_list_.reserve(capacity);
    for (std::size_t i = 0; i < capacity; ++i) {
      states_.pushBack(State{});
      free_list_.pushBack(i);
    }
    initialized_ = true;
  }

  void shutdown() {
    if (!initialized_) {
      return;
    }
    for (std::size_t i = 0; i < states_.size(); ++i) {
      State &state = states_[i];
      if (state.alive) {
        ops_->destroyHeap(state.heap);
        state.reset();
      }
    }
    states_.clear();
    free_list_.clear();
    key_to_index_.clear();
    device_ = nullptr;
    ops_ = nullptr;
    initialized_ = false;
  }

  std::size_t capacity() const noexcept { return states_.size(); }

  base::HeapId getOrCreate(const HeapDescriptorKey &key) {
    ensureInitialized();
    validateKey(key);
    if (auto it = key_to_index_.find(key); it != key_to_index_.end()) {
      const State &state = states_[it->second];
      return encodeId(it->second, state.generation);
    }
    const std::size_t index = allocateSlot();
    State &state = states_[index];
    state.key = key;
    state.heap = createHeap(key);
    state.alive = true;
    const auto id = encodeId(index, state.generation);
    key_to_index_.emplace(state.key, index);
    return id;
  }

  void release(base::HeapId id) {
    State &state = ensureAliveState(id);
    key_to_index_.erase(state.key);
    ops_->destroyHeap(state.heap);
    state.reset();
    ++state.generation;
    free_list_.pushBack(indexFromId(id));
  }

  ::orteaf::internal::backend::mps::MPSHeap_t getHeap(base::HeapId id) const {
    return ensureAliveState(id).heap;
  }

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

  DebugState debugState(base::HeapId id) const {
    DebugState snapshot{};
    snapshot.growth_chunk_size = growth_chunk_size_;
    const std::size_t index = indexFromId(id);
    if (index < states_.size()) {
      const State &state = states_[index];
      snapshot.alive = state.alive;
      snapshot.heap_allocated = state.heap != nullptr;
      snapshot.generation = state.generation;
      snapshot.size_bytes = state.key.size_bytes;
      snapshot.resource_options = state.key.resource_options;
      snapshot.storage_mode = state.key.storage_mode;
      snapshot.cpu_cache_mode = state.key.cpu_cache_mode;
      snapshot.hazard_tracking_mode = state.key.hazard_tracking_mode;
      snapshot.heap_type = state.key.heap_type;
    } else {
      snapshot.generation = std::numeric_limits<std::uint32_t>::max();
    }
    return snapshot;
  }
#endif

private:
  struct State {
    HeapDescriptorKey key{};
    ::orteaf::internal::backend::mps::MPSHeap_t heap{nullptr};
    std::uint32_t generation{0};
    bool alive{false};

    void reset() {
      key = HeapDescriptorKey{};
      heap = nullptr;
      alive = false;
    }
  };

  static constexpr std::uint32_t kGenerationBits = 8;
  static constexpr std::uint32_t kIndexBits = 24;
  static constexpr std::uint32_t kGenerationShift = kIndexBits;
  static constexpr std::uint32_t kIndexMask = (1u << kIndexBits) - 1u;
  static constexpr std::uint32_t kGenerationMask = (1u << kGenerationBits) - 1u;
  static constexpr std::size_t kMaxStateCount =
      static_cast<std::size_t>(kIndexMask);

  void ensureInitialized() const {
    if (!initialized_ || device_ == nullptr) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "MPS heap manager not initialized");
    }
  }

  void validateKey(const HeapDescriptorKey &key) const {
    if (key.size_bytes == 0) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          "Heap size must be > 0");
    }
  }

  State &ensureAliveState(base::HeapId id) {
    ensureInitialized();
    const std::size_t index = indexFromId(id);
    if (index >= states_.size()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          "MPS heap id out of range");
    }
    State &state = states_[index];
    if (!state.alive) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "MPS heap handle is inactive");
    }
    const std::uint32_t expected_generation = generationFromId(id);
    if ((state.generation & kGenerationMask) != expected_generation) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "MPS heap handle is stale");
    }
    return state;
  }

  const State &ensureAliveState(base::HeapId id) const {
    return const_cast<MpsHeapManager *>(this)->ensureAliveState(id);
  }

  std::size_t allocateSlot() {
    if (free_list_.empty()) {
      growStatePool(growth_chunk_size_);
      if (free_list_.empty()) {
        ::orteaf::internal::diagnostics::error::throwError(
            ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
            "No available MPS heap slots");
      }
    }
    const std::size_t index = free_list_.back();
    free_list_.resize(free_list_.size() - 1);
    return index;
  }

  void growStatePool(std::size_t additional) {
    if (additional == 0) {
      return;
    }
    if (additional > (kMaxStateCount - states_.size())) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          "Requested MPS heap capacity exceeds supported limit");
    }
    const std::size_t start = states_.size();
    states_.reserve(states_.size() + additional);
    free_list_.reserve(free_list_.size() + additional);
    for (std::size_t offset = 0; offset < additional; ++offset) {
      states_.pushBack(State{});
      free_list_.pushBack(start + offset);
    }
  }

  base::HeapId encodeId(std::size_t index, std::uint32_t generation) const {
    const std::uint32_t encoded_generation = generation & kGenerationMask;
    const std::uint32_t encoded = (encoded_generation << kGenerationShift) |
                                  static_cast<std::uint32_t>(index);
    return base::HeapId{encoded};
  }

  std::size_t indexFromId(base::HeapId id) const {
    return static_cast<std::size_t>(static_cast<std::uint32_t>(id) &
                                    kIndexMask);
  }

  std::uint32_t generationFromId(base::HeapId id) const {
    return (static_cast<std::uint32_t>(id) >> kGenerationShift) &
           kGenerationMask;
  }

  ::orteaf::internal::backend::mps::MPSHeap_t
  createHeap(const HeapDescriptorKey &key) {
    auto descriptor = ops_->createHeapDescriptor();
    if (descriptor == nullptr) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "Failed to allocate MPS heap descriptor");
    }
    struct DescriptorGuard {
      ::orteaf::internal::backend::mps::MPSHeapDescriptor_t handle{nullptr};
      BackendOps *ops{nullptr};
      ~DescriptorGuard() {
        if (handle != nullptr && ops != nullptr) {
          ops->destroyHeapDescriptor(handle);
        }
      }
    };
    DescriptorGuard guard{descriptor, ops_};
    ops_->setHeapDescriptorSize(descriptor, key.size_bytes);
    ops_->setHeapDescriptorResourceOptions(descriptor,
                                                 key.resource_options);
    ops_->setHeapDescriptorStorageMode(descriptor, key.storage_mode);
    ops_->setHeapDescriptorCPUCacheMode(descriptor, key.cpu_cache_mode);
    ops_->setHeapDescriptorHazardTrackingMode(descriptor,
                                                    key.hazard_tracking_mode);
    ops_->setHeapDescriptorType(descriptor, key.heap_type);
    auto heap = ops_->createHeap(device_, descriptor);
    ops_->destroyHeapDescriptor(descriptor);
    guard.handle = nullptr;
    if (heap == nullptr) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "Failed to create MPS heap");
    }
    return heap;
  }

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

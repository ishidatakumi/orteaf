#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>

#include "orteaf/internal/backend/mps/wrapper/mps_device.h"
#include "orteaf/internal/backend/mps/wrapper/mps_library.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/lease.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/runtime/manager/mps/mps_compute_pipeline_state_manager.h"

namespace orteaf::internal::runtime::mps {

enum class LibraryKeyKind : std::uint8_t {
  kNamed,
};

struct LibraryKey {
  LibraryKeyKind kind{LibraryKeyKind::kNamed};
  std::string identifier{};

  static LibraryKey Named(std::string identifier) {
    LibraryKey key{};
    key.kind = LibraryKeyKind::kNamed;
    key.identifier = std::move(identifier);
    return key;
  }

  friend bool operator==(const LibraryKey &lhs,
                         const LibraryKey &rhs) noexcept = default;
};

struct LibraryKeyHasher {
  std::size_t operator()(const LibraryKey &key) const noexcept {
    std::size_t seed = static_cast<std::size_t>(key.kind);
    constexpr std::size_t kMagic = 0x9e3779b97f4a7c15ull;
    seed ^= std::hash<std::string>{}(key.identifier) + kMagic + (seed << 6) +
            (seed >> 2);
    return seed;
  }
};

class MpsLibraryManager {
public:
  using BackendOps = ::orteaf::internal::runtime::backend_ops::mps::MpsSlowOps;
  using PipelineManager = MpsComputePipelineStateManager;
  using LibraryLease = ::orteaf::internal::base::Lease<
      ::orteaf::internal::base::LibraryHandle,
      ::orteaf::internal::backend::mps::MPSLibrary_t, MpsLibraryManager>;
  using PipelineManagerLease = ::orteaf::internal::base::Lease<
      ::orteaf::internal::base::LibraryHandle, PipelineManager *, MpsLibraryManager>;

  MpsLibraryManager() = default;
  MpsLibraryManager(const MpsLibraryManager&) = delete;
  MpsLibraryManager& operator=(const MpsLibraryManager&) = delete;
  MpsLibraryManager(MpsLibraryManager&&) = default;
  MpsLibraryManager& operator=(MpsLibraryManager&&) = default;
  ~MpsLibraryManager() = default;

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

  LibraryLease acquire(const LibraryKey &key);
  LibraryLease acquire(const PipelineManagerLease &pipeline_lease);
  PipelineManagerLease acquirePipelineManager(const LibraryLease &lease);
  PipelineManagerLease acquirePipelineManager(const LibraryKey &key);

  void release(LibraryLease &lease) noexcept;
  void release(PipelineManagerLease &lease) noexcept;

#if ORTEAF_ENABLE_TEST
  struct DebugState {
    bool alive{false};
    bool handle_allocated{false};
    std::uint32_t generation{0};
    std::uint32_t use_count{0};
    LibraryKeyKind kind{LibraryKeyKind::kNamed};
    std::string identifier{};
    std::size_t growth_chunk_size{0};
  };

  DebugState debugState(base::LibraryHandle id) const;
#endif

private:
  struct State {
    LibraryKey key{};
    ::orteaf::internal::backend::mps::MPSLibrary_t handle{nullptr};
    std::uint32_t generation{0};
    std::uint32_t use_count{0};
    bool alive{false};
    PipelineManager pipeline_manager{};

    void reset() {
      key = LibraryKey{};
      handle = nullptr;
      use_count = 0;
      alive = false;
    }
  };

  void ensureInitialized() const;

  void validateKey(const LibraryKey &key) const;

  State &ensureAliveState(base::LibraryHandle id);

  const State &ensureAliveState(base::LibraryHandle id) const {
    return const_cast<MpsLibraryManager *>(this)->ensureAliveState(id);
  }

  std::size_t allocateSlot();

  void growStatePool(std::size_t additional);

  base::LibraryHandle encodeId(std::size_t index, std::uint32_t generation) const;

  void releaseHandle(base::LibraryHandle id) noexcept;

  LibraryLease acquireLibraryFromHandle(base::LibraryHandle id);

  ::orteaf::internal::backend::mps::MPSLibrary_t
  createLibrary(const LibraryKey &key);

  ::orteaf::internal::base::HeapVector<State> states_{};
  ::orteaf::internal::base::HeapVector<std::size_t> free_list_{};
  std::unordered_map<LibraryKey, std::size_t, LibraryKeyHasher> key_to_index_{};
  std::size_t growth_chunk_size_{1};
  bool initialized_{false};
  ::orteaf::internal::backend::mps::MPSDevice_t device_{nullptr};
  BackendOps *ops_{nullptr};
};

} // namespace orteaf::internal::runtime::mps

#endif // ORTEAF_ENABLE_MPS

#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>

#include "orteaf/internal/runtime/mps/platform/wrapper/mps_device.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_library.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/lease.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/runtime/base/base_manager.h"
#include "orteaf/internal/runtime/mps/manager/mps_compute_pipeline_state_manager.h"

namespace orteaf::internal::runtime::mps::manager {

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

struct MpsLibraryManagerState {
  using PipelineManager = MpsComputePipelineStateManager;
  LibraryKey key{};
  ::orteaf::internal::runtime::mps::platform::wrapper::MPSLibrary_t handle{nullptr};
  bool alive{false};
  PipelineManager pipeline_manager{};
};

struct MpsLibraryManagerTraits {
  using DeviceType = ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t;
  using OpsType = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;
  using StateType = MpsLibraryManagerState;
  static constexpr const char *Name = "MPS library manager";
};

class MpsLibraryManager
    : public base::BaseManager<MpsLibraryManager, MpsLibraryManagerTraits> {
public:
  using SlowOps = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;
  using PipelineManager = MpsComputePipelineStateManager;
    using LibraryLease = ::orteaf::internal::base::Lease<
      ::orteaf::internal::base::LibraryHandle,
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSLibrary_t, MpsLibraryManager>;
  using PipelineManagerLease = ::orteaf::internal::base::Lease<
      ::orteaf::internal::base::LibraryHandle, PipelineManager *,
      MpsLibraryManager>;

  MpsLibraryManager() = default;
  MpsLibraryManager(const MpsLibraryManager &) = delete;
  MpsLibraryManager &operator=(const MpsLibraryManager &) = delete;
  MpsLibraryManager(MpsLibraryManager &&) = default;
  MpsLibraryManager &operator=(MpsLibraryManager &&) = default;
  ~MpsLibraryManager() = default;

  void initialize(::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t device,
                  SlowOps *slow_ops, std::size_t capacity);

  void shutdown();

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
    LibraryKeyKind kind{LibraryKeyKind::kNamed};
    std::string identifier{};
    std::size_t growth_chunk_size{0};
  };

  DebugState debugState(::orteaf::internal::base::LibraryHandle handle) const;
#endif

private:
  void validateKey(const LibraryKey &key) const;

  State &ensureAliveState(::orteaf::internal::base::LibraryHandle handle);

  const State &
  ensureAliveState(::orteaf::internal::base::LibraryHandle handle) const {
    return const_cast<MpsLibraryManager *>(this)->ensureAliveState(handle);
  }

  ::orteaf::internal::base::LibraryHandle encodeHandle(std::size_t index) const;

  void releaseHandle(::orteaf::internal::base::LibraryHandle handle) noexcept;

  LibraryLease
  acquireLibraryFromHandle(::orteaf::internal::base::LibraryHandle handle);

  ::orteaf::internal::runtime::mps::platform::wrapper::MPSLibrary_t
  createLibrary(const LibraryKey &key);

  std::unordered_map<LibraryKey, std::size_t, LibraryKeyHasher> key_to_index_{};
};

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

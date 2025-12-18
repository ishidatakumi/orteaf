#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/lease.h"
#include "orteaf/internal/runtime/base/lease/control_block/raw.h"
#include "orteaf/internal/runtime/base/lease/raw_lease.h"
#include "orteaf/internal/runtime/base/lease/slot.h"
#include "orteaf/internal/runtime/base/manager/base_manager_core.h"
#include "orteaf/internal/runtime/mps/manager/mps_compute_pipeline_state_manager.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_device.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_library.h"

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

// Resource struct: holds library + pipeline_manager
struct MpsLibraryResource {
  ::orteaf::internal::runtime::mps::platform::wrapper::MpsLibrary_t library{
      nullptr};
  MpsComputePipelineStateManager pipeline_manager{};
};

// ControlBlock type using RawControlBlock with RawSlot
using LibraryControlBlock = ::orteaf::internal::runtime::base::RawControlBlock<
    ::orteaf::internal::runtime::base::RawSlot<MpsLibraryResource>>;

// Traits for BaseManagerCore
struct MpsLibraryManagerTraits {
  using ControlBlock = LibraryControlBlock;
  using Handle = ::orteaf::internal::base::LibraryHandle;
  static constexpr const char *Name = "MPS library manager";
};

class MpsLibraryManager
    : public ::orteaf::internal::runtime::base::BaseManagerCore<
          MpsLibraryManagerTraits> {
public:
  using Base = ::orteaf::internal::runtime::base::BaseManagerCore<
      MpsLibraryManagerTraits>;
  using SlowOps = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;
  using DeviceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t;
  using PipelineManager = MpsComputePipelineStateManager;
  using LibraryHandle = ::orteaf::internal::base::LibraryHandle;
  using Handle = LibraryHandle;
  using LibraryLease = ::orteaf::internal::runtime::base::RawLease<
      LibraryHandle,
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsLibrary_t,
      MpsLibraryManager>;

  MpsLibraryManager() = default;
  MpsLibraryManager(const MpsLibraryManager &) = delete;
  MpsLibraryManager &operator=(const MpsLibraryManager &) = delete;
  MpsLibraryManager(MpsLibraryManager &&) = default;
  MpsLibraryManager &operator=(MpsLibraryManager &&) = default;
  ~MpsLibraryManager() = default;

  void initialize(DeviceType device, SlowOps *ops, std::size_t capacity);
  void shutdown();

  LibraryLease acquire(const LibraryKey &key);
  LibraryLease acquire(LibraryHandle handle);

  void release(LibraryLease &lease) noexcept;

  // Direct access to PipelineManager (no Lease pattern)
  PipelineManager *pipelineManager(const LibraryLease &lease);
  PipelineManager *pipelineManager(const LibraryKey &key);

  std::size_t capacity() const noexcept { return Base::capacity(); }

  using Base::growthChunkSize;
  using Base::isAlive;
  using Base::setGrowthChunkSize;

#if ORTEAF_ENABLE_TEST
  using Base::controlBlockForTest;
  using Base::freeListSizeForTest;
  using Base::isInitializedForTest;
#endif

private:
  friend LibraryLease;
  using Base::acquireExisting;

  void validateKey(const LibraryKey &key) const;

  ::orteaf::internal::runtime::mps::platform::wrapper::MpsLibrary_t
  createLibrary(const LibraryKey &key);

  void destroyResource(MpsLibraryResource &resource);

  std::unordered_map<LibraryKey, std::size_t, LibraryKeyHasher> key_to_index_{};
  DeviceType device_{nullptr};
  SlowOps *ops_{nullptr};
};

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

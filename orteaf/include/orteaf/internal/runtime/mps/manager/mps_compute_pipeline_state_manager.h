#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/runtime/base/lease/control_block/raw.h"
#include "orteaf/internal/runtime/base/lease/raw_lease.h"
#include "orteaf/internal/runtime/base/lease/slot.h"
#include "orteaf/internal/runtime/base/manager/base_manager_core.h"
#include "orteaf/internal/runtime/mps/platform/mps_slow_ops.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_compute_pipeline_state.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_function.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_library.h"

namespace orteaf::internal::runtime::mps::manager {

enum class FunctionKeyKind : std::uint8_t {
  kNamed,
};

struct FunctionKey {
  FunctionKeyKind kind{FunctionKeyKind::kNamed};
  std::string identifier{};

  static FunctionKey Named(std::string identifier) {
    FunctionKey key{};
    key.kind = FunctionKeyKind::kNamed;
    key.identifier = std::move(identifier);
    return key;
  }

  friend bool operator==(const FunctionKey &lhs,
                         const FunctionKey &rhs) noexcept = default;
};

struct FunctionKeyHasher {
  std::size_t operator()(const FunctionKey &key) const noexcept {
    std::size_t seed = static_cast<std::size_t>(key.kind);
    constexpr std::size_t kMagic = 0x9e3779b97f4a7c15ull;
    seed ^= std::hash<std::string>{}(key.identifier) + kMagic + (seed << 6) +
            (seed >> 2);
    return seed;
  }
};

// Resource struct: holds function + pipeline_state
struct MpsPipelineResource {
  ::orteaf::internal::runtime::mps::platform::wrapper::MPSFunction_t function{
      nullptr};
  ::orteaf::internal::runtime::mps::platform::wrapper::MPSComputePipelineState_t
      pipeline_state{nullptr};
};

// Slot type with initialization flag (lazy creation)
using PipelineSlot =
    ::orteaf::internal::runtime::base::Slot<MpsPipelineResource>;

// Control block type (Raw - no ref counting needed)
using PipelineControlBlock =
    ::orteaf::internal::runtime::base::RawControlBlock<PipelineSlot>;

/// @brief Traits for MpsComputePipelineStateManager
struct MpsComputePipelineStateManagerTraits {
  using ControlBlock = PipelineControlBlock;
  using Handle = ::orteaf::internal::base::FunctionHandle;
  static constexpr const char *Name = "MpsComputePipelineStateManager";
};

class MpsComputePipelineStateManager
    : protected ::orteaf::internal::runtime::base::BaseManagerCore<
          MpsComputePipelineStateManagerTraits> {
  using Base = ::orteaf::internal::runtime::base::BaseManagerCore<
      MpsComputePipelineStateManagerTraits>;

public:
  using SlowOps = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;
  using DeviceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t;
  using LibraryType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSLibrary_t;
  using FunctionHandle = ::orteaf::internal::base::FunctionHandle;
  using PipelineState = ::orteaf::internal::runtime::mps::platform::wrapper::
      MPSComputePipelineState_t;
  using PipelineLease = ::orteaf::internal::runtime::base::RawLease<
      FunctionHandle, PipelineState, MpsComputePipelineStateManager>;

  MpsComputePipelineStateManager() = default;
  MpsComputePipelineStateManager(const MpsComputePipelineStateManager &) =
      delete;
  MpsComputePipelineStateManager &
  operator=(const MpsComputePipelineStateManager &) = delete;
  MpsComputePipelineStateManager(MpsComputePipelineStateManager &&) = default;
  MpsComputePipelineStateManager &
  operator=(MpsComputePipelineStateManager &&) = default;
  ~MpsComputePipelineStateManager() = default;

  void initialize(DeviceType device, LibraryType library, SlowOps *ops,
                  std::size_t capacity);
  void shutdown();

  PipelineLease acquire(const FunctionKey &key);
  void release(PipelineLease &lease) noexcept;

  // Growth configuration
  void setGrowthChunkSize(std::size_t chunk) {
    if (chunk == 0) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          "Growth chunk size must be > 0");
    }
    growth_chunk_size_ = chunk;
  }
  std::size_t growthChunkSize() const noexcept { return growth_chunk_size_; }

  // Expose some base methods
  using Base::capacity;
  using Base::isInitialized;

#if ORTEAF_ENABLE_TEST
  using Base::controlBlockForTest;
  using Base::freeListSizeForTest;
  using Base::isInitializedForTest;

  std::size_t growthChunkSizeForTest() const noexcept {
    return growth_chunk_size_;
  }

  // Compatibility with old API: stateForTest returns control block reference
  auto &stateForTest(std::size_t index) { return controlBlockForTest(index); }
  const auto &stateForTest(std::size_t index) const {
    return controlBlockForTest(index);
  }
#endif

private:
  friend PipelineLease; // For release access

  void validateKey(const FunctionKey &key) const;
  void destroyResource(MpsPipelineResource &resource);

  std::unordered_map<FunctionKey, std::size_t, FunctionKeyHasher>
      key_to_index_{};
  LibraryType library_{nullptr};
  DeviceType device_{nullptr};
  SlowOps *ops_{nullptr};
  std::size_t growth_chunk_size_{1};
};

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

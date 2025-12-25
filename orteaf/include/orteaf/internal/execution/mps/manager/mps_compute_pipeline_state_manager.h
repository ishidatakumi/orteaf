#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/lease/control_block/weak.h"
#include "orteaf/internal/base/lease/weak_lease.h"
#include "orteaf/internal/base/manager/pool_manager.h"
#include "orteaf/internal/base/pool/fixed_slot_store.h"
#include "orteaf/internal/execution/mps/platform/mps_slow_ops.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_compute_pipeline_state.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_function.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_library.h"

namespace orteaf::internal::execution::mps::manager {

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
  ::orteaf::internal::execution::mps::platform::wrapper::MpsFunction_t function{
      nullptr};
  ::orteaf::internal::execution::mps::platform::wrapper::
      MpsComputePipelineState_t pipeline_state{nullptr};
};

struct PipelinePayloadPoolTraits {
  using Payload = MpsPipelineResource;
  using Handle = ::orteaf::internal::base::FunctionHandle;
  using DeviceType =
      ::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t;
  using LibraryType =
      ::orteaf::internal::execution::mps::platform::wrapper::MpsLibrary_t;
  using SlowOps = ::orteaf::internal::execution::mps::platform::MpsSlowOps;

  struct Request {
    FunctionKey key{};
  };

  struct Context {
    DeviceType device{nullptr};
    LibraryType library{nullptr};
    SlowOps *ops{nullptr};
  };

  static bool create(Payload &payload, const Request &request,
                     const Context &context) {
    if (context.ops == nullptr || context.device == nullptr ||
        context.library == nullptr) {
      return false;
    }
    payload.function =
        context.ops->createFunction(context.library, request.key.identifier);
    if (payload.function == nullptr) {
      return false;
    }
    payload.pipeline_state = context.ops->createComputePipelineState(
        context.device, payload.function);
    if (payload.pipeline_state == nullptr) {
      context.ops->destroyFunction(payload.function);
      payload.function = nullptr;
      return false;
    }
    return true;
  }

  static void destroy(Payload &payload, const Request &,
                      const Context &context) {
    if (payload.pipeline_state != nullptr && context.ops != nullptr) {
      context.ops->destroyComputePipelineState(payload.pipeline_state);
      payload.pipeline_state = nullptr;
    }
    if (payload.function != nullptr && context.ops != nullptr) {
      context.ops->destroyFunction(payload.function);
      payload.function = nullptr;
    }
  }
};

using PipelinePayloadPool =
    ::orteaf::internal::base::pool::FixedSlotStore<PipelinePayloadPoolTraits>;

struct PipelineControlBlockTag {};

using PipelineControlBlock = ::orteaf::internal::base::WeakControlBlock<
    ::orteaf::internal::base::FunctionHandle, MpsPipelineResource,
    PipelinePayloadPool>;

struct MpsComputePipelineStateManagerTraits {
  using PayloadPool = PipelinePayloadPool;
  using ControlBlock = PipelineControlBlock;
  struct ControlBlockTag {};
  using PayloadHandle = ::orteaf::internal::base::FunctionHandle;
  static constexpr const char *Name = "MpsComputePipelineStateManager";
};

class MpsComputePipelineStateManager {
  using Core = ::orteaf::internal::base::PoolManager<
      MpsComputePipelineStateManagerTraits>;

public:
  using SlowOps = ::orteaf::internal::execution::mps::platform::MpsSlowOps;
  using DeviceType =
      ::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t;
  using LibraryType =
      ::orteaf::internal::execution::mps::platform::wrapper::MpsLibrary_t;
  using FunctionHandle = ::orteaf::internal::base::FunctionHandle;
  using PipelineState = ::orteaf::internal::execution::mps::platform::wrapper::
      MpsComputePipelineState_t;
  using ControlBlockHandle = Core::ControlBlockHandle;
  using ControlBlockPool = Core::ControlBlockPool;
  using PipelineLease = Core::WeakLeaseType;

  MpsComputePipelineStateManager() = default;
  MpsComputePipelineStateManager(const MpsComputePipelineStateManager &) =
      delete;
  MpsComputePipelineStateManager &
  operator=(const MpsComputePipelineStateManager &) = delete;
  MpsComputePipelineStateManager(MpsComputePipelineStateManager &&) = default;
  MpsComputePipelineStateManager &
  operator=(MpsComputePipelineStateManager &&) = default;
  ~MpsComputePipelineStateManager() = default;

  struct Config {
    DeviceType device{nullptr};
    LibraryType library{nullptr};
    SlowOps *ops{nullptr};
    std::size_t payload_capacity{0};
    std::size_t control_block_capacity{0};
    std::size_t payload_block_size{0};
    std::size_t control_block_block_size{1};
    std::size_t payload_growth_chunk_size{1};
    std::size_t control_block_growth_chunk_size{1};
  };

  void configure(const Config &config);
  void shutdown();

  PipelineLease acquire(const FunctionKey &key);
  void release(PipelineLease &lease) noexcept { lease.release(); }

#if ORTEAF_ENABLE_TEST
  bool isConfiguredForTest() const noexcept { return core_.isConfigured(); }

  std::size_t payloadPoolSizeForTest() const noexcept {
    return core_.payloadPoolSizeForTest();
  }
  std::size_t payloadPoolCapacityForTest() const noexcept {
    return core_.payloadPoolCapacityForTest();
  }
  std::size_t controlBlockPoolSizeForTest() const noexcept {
    return core_.controlBlockPoolSizeForTest();
  }
  std::size_t controlBlockPoolCapacityForTest() const noexcept {
    return core_.controlBlockPoolCapacityForTest();
  }
  bool isAliveForTest(FunctionHandle handle) const noexcept {
    return core_.isAlive(handle);
  }
  std::size_t payloadGrowthChunkSizeForTest() const noexcept {
    return payload_growth_chunk_size_;
  }
  std::size_t controlBlockGrowthChunkSizeForTest() const noexcept {
    return core_.growthChunkSize();
  }

  bool payloadCreatedForTest(FunctionHandle handle) const noexcept {
    return core_.payloadCreatedForTest(handle);
  }

  const MpsPipelineResource *
  payloadForTest(FunctionHandle handle) const noexcept {
    return core_.payloadForTest(handle);
  }
#endif

private:
  friend PipelineLease;

  void validateKey(const FunctionKey &key) const;
  PipelinePayloadPoolTraits::Context makePayloadContext() const noexcept;

  std::unordered_map<FunctionKey, std::size_t, FunctionKeyHasher> key_to_index_{};
  LibraryType library_{nullptr};
  DeviceType device_{nullptr};
  SlowOps *ops_{nullptr};
  std::size_t payload_block_size_{0};
  std::size_t payload_growth_chunk_size_{1};
  Core core_{};
};

} // namespace orteaf::internal::execution::mps::manager

#endif // ORTEAF_ENABLE_MPS

#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/lease/control_block/weak.h"
#include "orteaf/internal/base/lease/weak_lease.h"
#include "orteaf/internal/base/manager/pool_manager.h"
#include "orteaf/internal/base/pool/fixed_slot_store.h"
#include "orteaf/internal/execution/mps/manager/mps_compute_pipeline_state_manager.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_device.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_library.h"

namespace orteaf::internal::execution::mps::manager {

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
  ::orteaf::internal::execution::mps::platform::wrapper::MpsLibrary_t library{
      nullptr};
  MpsComputePipelineStateManager pipeline_manager{};
};

// Payload pool
struct LibraryPayloadPoolTraits {
  using Payload = MpsLibraryResource;
  using Handle = ::orteaf::internal::base::LibraryHandle;
  using DeviceType =
      ::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t;
  using SlowOps = ::orteaf::internal::execution::mps::platform::MpsSlowOps;

  struct Request {
    LibraryKey key{};
  };

  struct Context {
    DeviceType device{nullptr};
    SlowOps *ops{nullptr};
    MpsComputePipelineStateManager::Config pipeline_config{};
  };

  static bool create(Payload &payload, const Request &request,
                     const Context &context) {
    if (context.ops == nullptr || context.device == nullptr) {
      return false;
    }
    payload.library = context.ops->createLibraryWithName(
        context.device, request.key.identifier);
    if (payload.library == nullptr) {
      return false;
    }
    auto pipeline_config = context.pipeline_config;
    pipeline_config.device = context.device;
    pipeline_config.library = payload.library;
    pipeline_config.ops = context.ops;
    payload.pipeline_manager.configure(pipeline_config);
    return true;
  }

  static void destroy(Payload &payload, const Request &,
                      const Context &context) {
    payload.pipeline_manager.shutdown();
    if (payload.library != nullptr && context.ops != nullptr) {
      context.ops->destroyLibrary(payload.library);
      payload.library = nullptr;
    }
  }
};

using LibraryPayloadPool =
    ::orteaf::internal::base::pool::FixedSlotStore<LibraryPayloadPoolTraits>;

// ControlBlock type using WeakControlBlock
using LibraryControlBlock = ::orteaf::internal::base::WeakControlBlock<
    ::orteaf::internal::base::LibraryHandle, MpsLibraryResource,
    LibraryPayloadPool>;

// Traits for PoolManager
struct MpsLibraryManagerTraits {
  using PayloadPool = LibraryPayloadPool;
  using ControlBlock = LibraryControlBlock;
  struct ControlBlockTag {};
  using PayloadHandle = ::orteaf::internal::base::LibraryHandle;
  static constexpr const char *Name = "MPS library manager";
};

class MpsLibraryManager {
public:
  using Core = ::orteaf::internal::base::PoolManager<MpsLibraryManagerTraits>;
  using SlowOps = ::orteaf::internal::execution::mps::platform::MpsSlowOps;
  using DeviceType =
      ::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t;
  using PipelineManager = MpsComputePipelineStateManager;
  using LibraryHandle = ::orteaf::internal::base::LibraryHandle;
  using ControlBlockHandle = Core::ControlBlockHandle;
  using ControlBlockPool = Core::ControlBlockPool;
  using LibraryLease = Core::WeakLeaseType;

  MpsLibraryManager() = default;
  MpsLibraryManager(const MpsLibraryManager &) = delete;
  MpsLibraryManager &operator=(const MpsLibraryManager &) = delete;
  MpsLibraryManager(MpsLibraryManager &&) = default;
  MpsLibraryManager &operator=(MpsLibraryManager &&) = default;
  ~MpsLibraryManager() = default;

  struct Config {
    DeviceType device{nullptr};
    SlowOps *ops{nullptr};
    std::size_t payload_capacity{0};
    std::size_t control_block_capacity{0};
    std::size_t payload_block_size{0};
    std::size_t control_block_block_size{1};
    std::size_t payload_growth_chunk_size{1};
    std::size_t control_block_growth_chunk_size{1};
    MpsComputePipelineStateManager::Config pipeline_config{};
  };

  void configure(const Config &config);
  void shutdown();

  LibraryLease acquire(const LibraryKey &key);
  LibraryLease acquire(LibraryHandle handle);

  void release(LibraryLease &lease) noexcept { lease.release(); }

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
  bool isAliveForTest(LibraryHandle handle) const noexcept {
    return core_.isAlive(handle);
  }
  std::size_t payloadGrowthChunkSizeForTest() const noexcept {
    return core_.payloadGrowthChunkSize();
  }
  std::size_t controlBlockGrowthChunkSizeForTest() const noexcept {
    return core_.growthChunkSize();
  }
  bool payloadCreatedForTest(LibraryHandle handle) const noexcept {
    return core_.payloadCreatedForTest(handle);
  }
  const MpsLibraryResource *payloadForTest(LibraryHandle handle) const noexcept {
    return core_.payloadForTest(handle);
  }
#endif

private:
  friend LibraryLease;

  void validateKey(const LibraryKey &key) const;
  LibraryPayloadPoolTraits::Context makePayloadContext() const noexcept;

  std::unordered_map<LibraryKey, std::size_t, LibraryKeyHasher> key_to_index_{};
  DeviceType device_{nullptr};
  SlowOps *ops_{nullptr};
  std::size_t payload_block_size_{0};
  MpsComputePipelineStateManager::Config pipeline_config_{};
  Core core_{};
};

} // namespace orteaf::internal::execution::mps::manager

#endif // ORTEAF_ENABLE_MPS

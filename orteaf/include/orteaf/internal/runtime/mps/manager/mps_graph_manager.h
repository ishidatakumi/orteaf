#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/runtime/base/lease/control_block/strong.h"
#include "orteaf/internal/runtime/base/lease/strong_lease.h"
#include "orteaf/internal/runtime/base/manager/base_pool_manager_core.h"
#include "orteaf/internal/runtime/base/pool/fixed_slot_store.h"
#include "orteaf/internal/runtime/mps/platform/mps_slow_ops.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_graph.h"

namespace orteaf::internal::runtime::mps::manager {

// =============================================================================
// Graph Key Types
// =============================================================================

enum class GraphKeyKind : std::uint8_t { kNamed };

struct GraphKey {
  GraphKeyKind kind{GraphKeyKind::kNamed};
  std::string identifier{};
  std::vector<std::int64_t> shape{};
  ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphDataType
      data_type{::orteaf::internal::runtime::mps::platform::wrapper::
                    MpsGraphDataType::kInvalid};
  std::size_t target_tensor_count{0};
  bool has_gradients{false};

  static GraphKey Named(std::string identifier) {
    GraphKey key{};
    key.kind = GraphKeyKind::kNamed;
    key.identifier = std::move(identifier);
    return key;
  }

  friend bool operator==(const GraphKey &lhs,
                         const GraphKey &rhs) noexcept = default;
};

struct GraphKeyHasher {
  std::size_t operator()(const GraphKey &key) const noexcept {
    std::size_t seed = static_cast<std::size_t>(key.kind);
    constexpr std::size_t kMagic = 0x9e3779b97f4a7c15ull;
    seed ^= std::hash<std::string>{}(key.identifier) + kMagic + (seed << 6) +
            (seed >> 2);
    seed ^= std::hash<std::size_t>{}(key.shape.size()) + kMagic + (seed << 6) +
            (seed >> 2);
    for (auto dim : key.shape) {
      seed ^=
          std::hash<std::int64_t>{}(dim) + kMagic + (seed << 6) + (seed >> 2);
    }
    seed ^=
        std::hash<std::uint32_t>{}(static_cast<std::uint32_t>(key.data_type)) +
        kMagic + (seed << 6) + (seed >> 2);
    seed ^= std::hash<std::size_t>{}(key.target_tensor_count) + kMagic +
            (seed << 6) + (seed >> 2);
    seed ^= std::hash<bool>{}(key.has_gradients) + kMagic + (seed << 6) +
            (seed >> 2);
    return seed;
  }
};

// =============================================================================
// Graph Resource
// =============================================================================

struct MpsGraphResource {
  ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraph_t graph{
      nullptr};
  ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphExecutable_t
      executable{nullptr};
};

// =============================================================================
// Payload Pool
// =============================================================================

struct GraphPayloadPoolTraits {
  using Payload = MpsGraphResource;
  using Handle = ::orteaf::internal::base::GraphHandle;
  using DeviceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t;
  using SlowOps = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;

  using CompileFn = std::function<
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphExecutable_t(
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraph_t graph,
          DeviceType device, SlowOps *slow_ops)>;

  struct Request {
    const CompileFn *compile_fn{nullptr};
  };

  struct Context {
    DeviceType device{nullptr};
    SlowOps *ops{nullptr};
  };

  static bool create(Payload &payload, const Request &request,
                     const Context &context) {
    if (context.ops == nullptr || context.device == nullptr ||
        request.compile_fn == nullptr) {
      return false;
    }
    payload.graph = context.ops->createGraph();
    payload.executable =
        (*request.compile_fn)(payload.graph, context.device, context.ops);
    if (payload.executable == nullptr) {
      if (payload.graph != nullptr) {
        context.ops->destroyGraph(payload.graph);
      }
      payload.graph = nullptr;
      return false;
    }
    return true;
  }

  static void destroy(Payload &payload, const Request &,
                      const Context &context) {
    if (payload.executable != nullptr && context.ops != nullptr) {
      context.ops->destroyGraphExecutable(payload.executable);
      payload.executable = nullptr;
    }
    if (payload.graph != nullptr && context.ops != nullptr) {
      context.ops->destroyGraph(payload.graph);
      payload.graph = nullptr;
    }
  }
};

using GraphPayloadPool =
    ::orteaf::internal::runtime::base::pool::FixedSlotStore<
        GraphPayloadPoolTraits>;

// =============================================================================
// ControlBlock
// =============================================================================

struct GraphControlBlockTag {};

using GraphControlBlock = ::orteaf::internal::runtime::base::StrongControlBlock<
    ::orteaf::internal::base::GraphHandle, MpsGraphResource, GraphPayloadPool>;

// =============================================================================
// Manager Traits for BasePoolManagerCore
// =============================================================================

struct MpsGraphManagerTraits {
  using PayloadPool = GraphPayloadPool;
  using ControlBlock = GraphControlBlock;
  struct ControlBlockTag {};
  using PayloadHandle = ::orteaf::internal::base::GraphHandle;
  static constexpr const char *Name = "MpsGraphManager";
};

// =============================================================================
// MpsGraphManager
// =============================================================================

class MpsGraphManager {
  using Core = ::orteaf::internal::runtime::base::BasePoolManagerCore<
      MpsGraphManagerTraits>;

public:
  using SlowOps = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;
  using DeviceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t;
  using GraphHandle = ::orteaf::internal::base::GraphHandle;
  using ExecutableType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphExecutable_t;
  using ControlBlockHandle = Core::ControlBlockHandle;
  using ControlBlockPool = Core::ControlBlockPool;
  using GraphLease = ::orteaf::internal::runtime::base::StrongLease<
      ControlBlockHandle, GraphControlBlock, ControlBlockPool,
      MpsGraphManager>;
  using CompileFn = std::function<ExecutableType(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraph_t graph,
      DeviceType device, SlowOps *slow_ops)>;

private:
  friend GraphLease;

public:
  struct Config {
    DeviceType device{nullptr};
    SlowOps *ops{nullptr};
    std::size_t payload_capacity{0};
    std::size_t control_block_capacity{0};
    std::size_t payload_block_size{0};
    std::size_t control_block_block_size{1};
    std::size_t payload_growth_chunk_size{1};
    std::size_t control_block_growth_chunk_size{1};
  };

  MpsGraphManager() = default;
  MpsGraphManager(const MpsGraphManager &) = delete;
  MpsGraphManager &operator=(const MpsGraphManager &) = delete;
  MpsGraphManager(MpsGraphManager &&) = default;
  MpsGraphManager &operator=(MpsGraphManager &&) = default;
  ~MpsGraphManager() = default;

  void configure(const Config &config);
  void shutdown();

  GraphLease acquire(const GraphKey &key, const CompileFn &compile_fn);
  void release(GraphLease &lease) noexcept { lease.release(); }

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
  bool isAliveForTest(GraphHandle handle) const noexcept {
    return core_.isAlive(handle);
  }
  std::size_t payloadGrowthChunkSizeForTest() const noexcept {
    return payload_growth_chunk_size_;
  }
  std::size_t controlBlockGrowthChunkSizeForTest() const noexcept {
    return core_.growthChunkSize();
  }
#endif

private:
  void validateKey(const GraphKey &key) const;
  GraphLease buildLease(GraphHandle handle, MpsGraphResource *payload_ptr);
  GraphPayloadPoolTraits::Context makePayloadContext() const noexcept;

  std::unordered_map<GraphKey, std::size_t, GraphKeyHasher> key_to_index_{};
  DeviceType device_{nullptr};
  SlowOps *ops_{nullptr};
  std::size_t payload_block_size_{0};
  std::size_t payload_growth_chunk_size_{1};
  std::size_t next_index_{0};
  Core core_{};
};

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "orteaf/internal/runtime/mps/platform/wrapper/mps_graph.h"
#include "orteaf/internal/runtime/mps/platform/mps_slow_ops.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/lease.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/runtime/base/base_manager.h"

namespace orteaf::internal::runtime::mps::manager {

enum class GraphKeyKind : std::uint8_t { kNamed };

struct GraphKey {
  GraphKeyKind kind{GraphKeyKind::kNamed};
  std::string identifier{};
  std::vector<std::int64_t> shape{};
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphDataType data_type{
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphDataType::kInvalid};
  std::size_t target_tensor_count{0};
  bool has_gradients{false};

  static GraphKey Named(std::string identifier) {
    GraphKey key{};
    key.kind = GraphKeyKind::kNamed;
    key.identifier = std::move(identifier);
    return key;
  }

  friend bool operator==(const GraphKey& lhs,
                         const GraphKey& rhs) noexcept = default;
};

struct GraphKeyHasher {
  std::size_t operator()(const GraphKey& key) const noexcept {
    std::size_t seed = static_cast<std::size_t>(key.kind);
    constexpr std::size_t kMagic = 0x9e3779b97f4a7c15ull;
    seed ^= std::hash<std::string>{}(key.identifier) + kMagic + (seed << 6) +
            (seed >> 2);
    seed ^= std::hash<std::size_t>{}(key.shape.size()) + kMagic + (seed << 6) +
            (seed >> 2);
    for (auto dim : key.shape) {
      seed ^= std::hash<std::int64_t>{}(dim) + kMagic + (seed << 6) +
              (seed >> 2);
    }
    seed ^= std::hash<std::uint32_t>{}(
                static_cast<std::uint32_t>(key.data_type)) +
            kMagic + (seed << 6) + (seed >> 2);
    seed ^= std::hash<std::size_t>{}(key.target_tensor_count) + kMagic +
            (seed << 6) + (seed >> 2);
    seed ^= std::hash<bool>{}(key.has_gradients) + kMagic + (seed << 6) +
            (seed >> 2);
    return seed;
  }
};

struct MpsGraphManagerState;

struct MpsGraphManagerTraits {
  using DeviceType = ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t;
  using OpsType = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;
  using StateType = struct MpsGraphManagerState;
  static constexpr const char* Name = "MPS graph manager";
};

struct MpsGraphManagerState {
  GraphKey key{};
  ::orteaf::internal::runtime::mps::platform::wrapper::MPSGraph_t graph{nullptr};
  ::orteaf::internal::runtime::mps::platform::wrapper::MPSGraphExecutable_t executable{nullptr};
  std::uint32_t generation{0};
  bool alive{false};
};

class MpsGraphManager
    : public base::BaseManager<MpsGraphManager, MpsGraphManagerTraits> {
public:
  using SlowOps = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;
    using GraphLease = ::orteaf::internal::base::Lease<
      ::orteaf::internal::base::GraphHandle,
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSGraphExecutable_t, MpsGraphManager>;
    using CompileFn = std::function<::orteaf::internal::runtime::mps::platform::wrapper::MPSGraphExecutable_t(
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSGraph_t graph,
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t device, SlowOps* slow_ops)>;

  MpsGraphManager() = default;
  MpsGraphManager(const MpsGraphManager&) = delete;
  MpsGraphManager& operator=(const MpsGraphManager&) = delete;
  MpsGraphManager(MpsGraphManager&&) = default;
  MpsGraphManager& operator=(MpsGraphManager&&) = default;
  ~MpsGraphManager() = default;

  void initialize(::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t device,
                  SlowOps* slow_ops, std::size_t capacity);

  void shutdown();

  GraphLease acquire(const GraphKey& key, const CompileFn& compile_fn);

  void release(GraphLease& lease) noexcept;

#if ORTEAF_ENABLE_TEST
  struct DebugState {
    bool alive{false};
    bool graph_allocated{false};
    bool executable_allocated{false};
    GraphKeyKind kind{GraphKeyKind::kNamed};
    std::string identifier{};
    std::uint32_t generation{0};
    std::size_t growth_chunk_size{0};
    std::vector<std::int64_t> shape{};
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphDataType data_type{
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphDataType::kInvalid};
    std::size_t target_tensor_count{0};
    bool has_gradients{false};
  };

  DebugState debugState(::orteaf::internal::base::GraphHandle handle) const;
#endif

private:
  void validateKey(const GraphKey& key) const;

  ::orteaf::internal::base::GraphHandle encodeHandle(std::size_t index,
                                                     std::uint32_t generation) const;

  void destroyState(MpsGraphManagerState& state);

  MpsGraphManagerState&
  ensureAliveState(::orteaf::internal::base::GraphHandle handle);

  const MpsGraphManagerState&
  ensureAliveState(::orteaf::internal::base::GraphHandle handle) const {
    return const_cast<MpsGraphManager*>(this)->ensureAliveState(handle);
  }

  std::unordered_map<GraphKey, std::size_t, GraphKeyHasher> key_to_index_{};
};

}  // namespace orteaf::internal::runtime::mps

#endif  // ORTEAF_ENABLE_MPS

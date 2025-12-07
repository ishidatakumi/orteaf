#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "orteaf/internal/backend/mps/wrapper/mps_compute_pipeline_state.h"
#include "orteaf/internal/backend/mps/wrapper/mps_function.h"
#include "orteaf/internal/backend/mps/wrapper/mps_library.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/lease.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/backend/mps/mps_slow_ops.h"
#include "orteaf/internal/runtime/base/base_manager.h"

namespace orteaf::internal::runtime::mps {

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

    friend bool operator==(const FunctionKey& lhs, const FunctionKey& rhs) noexcept = default;
};

struct FunctionKeyHasher {
    std::size_t operator()(const FunctionKey& key) const noexcept {
        std::size_t seed = static_cast<std::size_t>(key.kind);
        constexpr std::size_t kMagic = 0x9e3779b97f4a7c15ull;
        seed ^= std::hash<std::string>{}(key.identifier) + kMagic + (seed << 6) + (seed >> 2);
        return seed;
    }
};

struct MpsComputePipelineStateManagerState {
    FunctionKey key{};
    ::orteaf::internal::backend::mps::MPSFunction_t function{nullptr};
    ::orteaf::internal::backend::mps::MPSComputePipelineState_t pipeline_state{nullptr};
    std::uint32_t generation{0};
    std::uint32_t use_count{0};
    bool alive{false};
};

struct MpsComputePipelineStateManagerTraits {
    using DeviceType = ::orteaf::internal::backend::mps::MPSDevice_t;
    using OpsType = ::orteaf::internal::runtime::backend_ops::mps::MpsSlowOps;
    using StateType = MpsComputePipelineStateManagerState;
    static constexpr const char *Name = "MPS compute pipeline state manager";
};

class MpsComputePipelineStateManager : public base::BaseManager<MpsComputePipelineStateManager, MpsComputePipelineStateManagerTraits> {
public:
    using SlowOps = ::orteaf::internal::runtime::backend_ops::mps::MpsSlowOps;
    using PipelineLease = ::orteaf::internal::base::Lease<
        ::orteaf::internal::base::FunctionHandle,
        ::orteaf::internal::backend::mps::MPSComputePipelineState_t,
        MpsComputePipelineStateManager>;

    MpsComputePipelineStateManager() = default;
    MpsComputePipelineStateManager(const MpsComputePipelineStateManager&) = delete;
    MpsComputePipelineStateManager& operator=(const MpsComputePipelineStateManager&) = delete;
    MpsComputePipelineStateManager(MpsComputePipelineStateManager&&) = default;
    MpsComputePipelineStateManager& operator=(MpsComputePipelineStateManager&&) = default;
    ~MpsComputePipelineStateManager() = default;

    void initialize(::orteaf::internal::backend::mps::MPSDevice_t device,
                    ::orteaf::internal::backend::mps::MPSLibrary_t library,
                    SlowOps *slow_ops,
                    std::size_t capacity);

    void shutdown();

    PipelineLease acquire(const FunctionKey& key);
    void release(PipelineLease& lease) noexcept;

#if ORTEAF_ENABLE_TEST
    struct DebugState {
        bool alive{false};
        bool pipeline_allocated{false};
        bool function_allocated{false};
        std::uint32_t generation{0};
        std::uint32_t use_count{0};
        FunctionKeyKind kind{FunctionKeyKind::kNamed};
        std::string identifier{};
        std::size_t growth_chunk_size{0};
    };

    DebugState debugState(::orteaf::internal::base::FunctionHandle handle) const;
#endif

private:
    void validateKey(const FunctionKey& key) const;

    void destroyState(State& state);

    State& ensureAliveState(::orteaf::internal::base::FunctionHandle handle);

    const State& ensureAliveState(::orteaf::internal::base::FunctionHandle handle) const {
        return const_cast<MpsComputePipelineStateManager*>(this)->ensureAliveState(handle);
    }

    ::orteaf::internal::base::FunctionHandle encodeHandle(std::size_t index, std::uint32_t generation) const;
    void releaseHandle(::orteaf::internal::base::FunctionHandle handle) noexcept;

    std::unordered_map<FunctionKey, std::size_t, FunctionKeyHasher> key_to_index_{};
    ::orteaf::internal::backend::mps::MPSLibrary_t library_{nullptr};
};

}  // namespace orteaf::internal::runtime::mps

#endif // ORTEAF_ENABLE_MPS

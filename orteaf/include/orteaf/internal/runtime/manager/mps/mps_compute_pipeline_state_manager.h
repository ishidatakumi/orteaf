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

#include "orteaf/internal/backend/mps/mps_compute_pipeline_state.h"
#include "orteaf/internal/backend/mps/mps_function.h"
#include "orteaf/internal/backend/mps/mps_library.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/base/strong_id.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/backend/mps/mps_slow_ops.h"

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

class MpsComputePipelineStateManager {
public:
    using BackendOps = ::orteaf::internal::runtime::backend_ops::mps::MpsSlowOps;

    MpsComputePipelineStateManager() = default;
    MpsComputePipelineStateManager(const MpsComputePipelineStateManager&) = delete;
    MpsComputePipelineStateManager& operator=(const MpsComputePipelineStateManager&) = delete;
    MpsComputePipelineStateManager(MpsComputePipelineStateManager&&) = default;
    MpsComputePipelineStateManager& operator=(MpsComputePipelineStateManager&&) = default;
    ~MpsComputePipelineStateManager() = default;

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
                    ::orteaf::internal::backend::mps::MPSLibrary_t library,
                    BackendOps *ops,
                    std::size_t capacity);

    void shutdown();

    std::size_t capacity() const noexcept { return states_.size(); }

    base::FunctionId getOrCreate(const FunctionKey& key);

    void release(base::FunctionId id);

    ::orteaf::internal::backend::mps::MPSComputePipelineState_t getPipelineState(base::FunctionId id) const;

    ::orteaf::internal::backend::mps::MPSFunction_t getFunction(base::FunctionId id) const;

#if ORTEAF_ENABLE_TEST
    struct DebugState {
        bool alive{false};
        bool pipeline_allocated{false};
        bool function_allocated{false};
        std::uint32_t generation{0};
        FunctionKeyKind kind{FunctionKeyKind::kNamed};
        std::string identifier{};
        std::size_t growth_chunk_size{0};
    };

    DebugState debugState(base::FunctionId id) const;
#endif

private:
    struct State {
        FunctionKey key{};
        ::orteaf::internal::backend::mps::MPSFunction_t function{nullptr};
        ::orteaf::internal::backend::mps::MPSComputePipelineState_t pipeline_state{nullptr};
        std::uint32_t generation{0};
        bool alive{false};
    };

    static constexpr std::uint32_t kGenerationBits = 8;
    static constexpr std::uint32_t kIndexBits = 24;
    static constexpr std::uint32_t kGenerationShift = kIndexBits;
    static constexpr std::uint32_t kIndexMask = (1u << kIndexBits) - 1u;
    static constexpr std::uint32_t kGenerationMask = (1u << kGenerationBits) - 1u;
    static constexpr std::size_t kMaxStateCount = static_cast<std::size_t>(kIndexMask);

    void ensureInitialized() const;

    void validateKey(const FunctionKey& key) const;

    void destroyState(State& state);

    State& ensureAliveState(base::FunctionId id);

    const State& ensureAliveState(base::FunctionId id) const {
        return const_cast<MpsComputePipelineStateManager*>(this)->ensureAliveState(id);
    }

    std::size_t allocateSlot();

    void growStatePool(std::size_t additional);

    base::FunctionId encodeId(std::size_t index, std::uint32_t generation) const;

    std::size_t indexFromId(base::FunctionId id) const;

    std::uint32_t generationFromId(base::FunctionId id) const;

    ::orteaf::internal::base::HeapVector<State> states_{};
    ::orteaf::internal::base::HeapVector<std::size_t> free_list_{};
    std::unordered_map<FunctionKey, std::size_t, FunctionKeyHasher> key_to_index_{};
    std::size_t growth_chunk_size_{1};
    bool initialized_{false};
    ::orteaf::internal::backend::mps::MPSDevice_t device_{nullptr};
    ::orteaf::internal::backend::mps::MPSLibrary_t library_{nullptr};
    BackendOps *ops_{nullptr};
};

}  // namespace orteaf::internal::runtime::mps

#endif // ORTEAF_ENABLE_MPS

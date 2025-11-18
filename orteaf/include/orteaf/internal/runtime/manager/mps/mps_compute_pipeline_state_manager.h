#pragma once

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
#include "orteaf/internal/runtime/backend_ops/mps/mps_backend_ops.h"
#include "orteaf/internal/runtime/backend_ops/mps/mps_backend_ops_concepts.h"

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

template <class BackendOps = ::orteaf::internal::runtime::backend_ops::mps::MpsBackendOps>
requires ::orteaf::internal::runtime::backend_ops::mps::MpsRuntimeBackendOps<BackendOps>
class MpsComputePipelineStateManager {
public:
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
                    std::size_t capacity) {
        shutdown();
        if (device == nullptr || library == nullptr) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
                "MPS compute pipeline state manager requires a valid device and library");
        }
        if (capacity > kMaxStateCount) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
                "Requested MPS compute pipeline capacity exceeds supported limit");
        }
        device_ = device;
        library_ = library;
        states_.clear();
        free_list_.clear();
        key_to_index_.clear();
        states_.reserve(capacity);
        free_list_.reserve(capacity);
        for (std::size_t i = 0; i < capacity; ++i) {
            states_.emplaceBack();
            free_list_.pushBack(i);
        }
        initialized_ = true;
    }

    void shutdown() {
        if (!initialized_) {
            return;
        }
        for (std::size_t i = 0; i < states_.size(); ++i) {
            destroyState(states_[i]);
        }
        states_.clear();
        free_list_.clear();
        key_to_index_.clear();
        device_ = nullptr;
        library_ = nullptr;
        initialized_ = false;
    }

    std::size_t capacity() const noexcept { return states_.size(); }

    base::FunctionId getOrCreate(const FunctionKey& key) {
        ensureInitialized();
        validateKey(key);
        if (auto it = key_to_index_.find(key); it != key_to_index_.end()) {
            const State& state = states_[it->second];
            return encodeId(it->second, state.generation);
        }
        const std::size_t index = allocateSlot();
        State& state = states_[index];
        state.key = key;
        state.function = BackendOps::createFunction(library_, key.identifier);
        if (state.function == nullptr) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
                "Failed to create MPS function for compute pipeline");
        }
        state.pipeline_state = BackendOps::createComputePipelineState(device_, state.function);
        if (state.pipeline_state == nullptr) {
            BackendOps::destroyFunction(state.function);
            state.function = nullptr;
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
                "Failed to create MPS compute pipeline state");
        }
        state.alive = true;
        const auto id = encodeId(index, state.generation);
        key_to_index_.emplace(state.key, index);
        return id;
    }

    void release(base::FunctionId id) {
        State& state = ensureAliveState(id);
        key_to_index_.erase(state.key);
        destroyState(state);
        ++state.generation;
        free_list_.pushBack(indexFromId(id));
    }

    ::orteaf::internal::backend::mps::MPSComputePipelineState_t getPipelineState(base::FunctionId id) const {
        return ensureAliveState(id).pipeline_state;
    }

    ::orteaf::internal::backend::mps::MPSFunction_t getFunction(base::FunctionId id) const {
        return ensureAliveState(id).function;
    }

#if ORTEAF_ENABLE_TEST
    struct DebugState {
        bool alive{false};
        bool pipeline_allocated{false};
        bool function_allocated{false};
        std::uint32_t generation{0};
        FunctionKeyKind kind{FunctionKeyKind::kNamed};
        std::string identifier{};
    };

    DebugState debugState(base::FunctionId id) const {
        DebugState snapshot{};
        const std::size_t index = indexFromId(id);
        if (index < states_.size()) {
            const State& state = states_[index];
            snapshot.alive = state.alive;
            snapshot.pipeline_allocated = state.pipeline_state != nullptr;
            snapshot.function_allocated = state.function != nullptr;
            snapshot.generation = state.generation;
            snapshot.kind = state.key.kind;
            snapshot.identifier = state.key.identifier;
        } else {
            snapshot.generation = std::numeric_limits<std::uint32_t>::max();
        }
        return snapshot;
    }
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

    void ensureInitialized() const {
        if (!initialized_ || device_ == nullptr || library_ == nullptr) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
                "MPS compute pipeline state manager not initialized");
        }
    }

    void validateKey(const FunctionKey& key) const {
        if (key.identifier.empty()) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
                "Function identifier cannot be empty");
        }
    }

    std::size_t allocateSlot() {
        if (free_list_.empty()) {
            growStatePool(growth_chunk_size_);
            if (free_list_.empty()) {
                ::orteaf::internal::diagnostics::error::throwError(
                    ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
                    "No available MPS compute pipeline slots");
            }
        }
        const std::size_t index = free_list_.back();
        free_list_.resize(free_list_.size() - 1);
        return index;
    }

    void growStatePool(std::size_t additional) {
        if (additional == 0) {
            return;
        }
        if (additional > (kMaxStateCount - states_.size())) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
                "Requested MPS compute pipeline capacity exceeds supported limit");
        }
        const std::size_t start = states_.size();
        states_.reserve(states_.size() + additional);
        free_list_.reserve(free_list_.size() + additional);
        for (std::size_t offset = 0; offset < additional; ++offset) {
            states_.emplaceBack();
            free_list_.pushBack(start + offset);
        }
    }

    void destroyState(State& state) {
        if (state.pipeline_state != nullptr) {
            BackendOps::destroyComputePipelineState(state.pipeline_state);
            state.pipeline_state = nullptr;
        }
        if (state.function != nullptr) {
            BackendOps::destroyFunction(state.function);
            state.function = nullptr;
        }
        state.alive = false;
    }

    State& ensureAliveState(base::FunctionId id) {
        ensureInitialized();
        const std::size_t index = indexFromId(id);
        if (index >= states_.size()) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
                "MPS function id out of range");
        }
        State& state = states_[index];
        if (!state.alive) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
                "MPS compute pipeline state is inactive");
        }
        const std::uint32_t expected_generation = generationFromId(id);
        if ((state.generation & kGenerationMask) != expected_generation) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
                "MPS compute pipeline handle is stale");
        }
        return state;
    }

    const State& ensureAliveState(base::FunctionId id) const {
        return const_cast<MpsComputePipelineStateManager*>(this)->ensureAliveState(id);
    }

    base::FunctionId encodeId(std::size_t index, std::uint32_t generation) const {
        const std::uint32_t encoded_generation = generation & kGenerationMask;
        const std::uint32_t encoded =
            (encoded_generation << kGenerationShift) |
            static_cast<std::uint32_t>(index);
        return base::FunctionId{encoded};
    }

    std::size_t indexFromId(base::FunctionId id) const {
        return static_cast<std::size_t>(static_cast<std::uint32_t>(id) & kIndexMask);
    }

    std::uint32_t generationFromId(base::FunctionId id) const {
        return (static_cast<std::uint32_t>(id) >> kGenerationShift) & kGenerationMask;
    }

    ::orteaf::internal::base::HeapVector<State> states_{};
    ::orteaf::internal::base::HeapVector<std::size_t> free_list_{};
    std::unordered_map<FunctionKey, std::size_t, FunctionKeyHasher> key_to_index_{};
    std::size_t growth_chunk_size_{1};
    bool initialized_{false};
    ::orteaf::internal::backend::mps::MPSDevice_t device_{nullptr};
    ::orteaf::internal::backend::mps::MPSLibrary_t library_{nullptr};
};

}  // namespace orteaf::internal::runtime::mps

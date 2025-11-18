#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>

#include "orteaf/internal/backend/mps/mps_device.h"
#include "orteaf/internal/backend/mps/mps_library.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/base/strong_id.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/runtime/backend_ops/mps/mps_backend_ops.h"
#include "orteaf/internal/runtime/backend_ops/mps/mps_backend_ops_concepts.h"
#include "orteaf/internal/runtime/manager/mps/mps_compute_pipeline_state_manager.h"

namespace orteaf::internal::runtime::mps {

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

    friend bool operator==(const LibraryKey& lhs, const LibraryKey& rhs) noexcept = default;
};

struct LibraryKeyHasher {
    std::size_t operator()(const LibraryKey& key) const noexcept {
        std::size_t seed = static_cast<std::size_t>(key.kind);
        constexpr std::size_t kMagic = 0x9e3779b97f4a7c15ull;
        seed ^= std::hash<std::string>{}(key.identifier) + kMagic + (seed << 6) + (seed >> 2);
        return seed;
    }
};

template <class BackendOps = ::orteaf::internal::runtime::backend_ops::mps::MpsBackendOps>
requires ::orteaf::internal::runtime::backend_ops::mps::MpsRuntimeBackendOps<BackendOps>
class MpsLibraryManager {
public:
    using PipelineManager = MpsComputePipelineStateManager<BackendOps>;

    void setGrowthChunkSize(std::size_t chunk) {
        if (chunk == 0) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
                "Growth chunk size must be > 0");
        }
        growth_chunk_size_ = chunk;
    }

    std::size_t growthChunkSize() const noexcept { return growth_chunk_size_; }

    void initialize(::orteaf::internal::backend::mps::MPSDevice_t device, std::size_t capacity) {
        shutdown();
        if (device == nullptr) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
                "MPS library manager requires a valid device");
        }
        device_ = device;
        if (capacity > kMaxStateCount) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
                "Requested MPS library capacity exceeds supported limit");
        }
        states_.clear();
        free_list_.clear();
        key_to_index_.clear();
        states_.reserve(capacity);
        free_list_.reserve(capacity);
        for (std::size_t i = 0; i < capacity; ++i) {
            states_.pushBack(State{});
            free_list_.pushBack(i);
        }
        initialized_ = true;
    }

    void shutdown() {
        if (!initialized_) {
            return;
        }
        for (std::size_t i = 0; i < states_.size(); ++i) {
            State& state = states_[i];
            if (state.alive) {
                state.pipeline_manager.shutdown();
                BackendOps::destroyLibrary(state.handle);
                state.reset();
            }
        }
        states_.clear();
        free_list_.clear();
        key_to_index_.clear();
        device_ = nullptr;
        initialized_ = false;
    }

    std::size_t capacity() const noexcept { return states_.size(); }

    base::LibraryId getOrCreate(const LibraryKey& key) {
        ensureInitialized();
        validateKey(key);
        if (auto it = key_to_index_.find(key); it != key_to_index_.end()) {
            const State& state = states_[it->second];
            return encodeId(it->second, state.generation);
        }
        const std::size_t index = allocateSlot();
        State& state = states_[index];
        state.handle = createLibrary(key);
        state.key = key;
        state.pipeline_manager.initialize(device_, state.handle, 0);
        state.alive = true;
        const auto id = encodeId(index, state.generation);
        key_to_index_.emplace(state.key, index);
        return id;
    }

    void release(base::LibraryId id) {
        State& state = ensureAliveState(id);
        key_to_index_.erase(state.key);
        state.pipeline_manager.shutdown();
        BackendOps::destroyLibrary(state.handle);
        state.reset();
        ++state.generation;
        free_list_.pushBack(indexFromId(id));
    }

    ::orteaf::internal::backend::mps::MPSLibrary_t getLibrary(base::LibraryId id) const {
        return ensureAliveState(id).handle;
    }

    PipelineManager& pipelineManager(base::LibraryId id) {
        State& state = ensureAliveState(id);
        return state.pipeline_manager;
    }

    const PipelineManager& pipelineManager(base::LibraryId id) const {
        const State& state = ensureAliveState(id);
        return state.pipeline_manager;
    }

#if ORTEAF_ENABLE_TEST
    struct DebugState {
        bool alive{false};
        bool handle_allocated{false};
        std::uint32_t generation{0};
        LibraryKeyKind kind{LibraryKeyKind::kNamed};
        std::string identifier{};
    };

    DebugState debugState(base::LibraryId id) const {
        DebugState snapshot{};
        const std::size_t index = indexFromId(id);
        if (index < states_.size()) {
            const State& state = states_[index];
            snapshot.alive = state.alive;
            snapshot.handle_allocated = state.handle != nullptr;
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
        LibraryKey key{};
        ::orteaf::internal::backend::mps::MPSLibrary_t handle{nullptr};
        std::uint32_t generation{0};
        bool alive{false};
        PipelineManager pipeline_manager{};

        void reset() {
            key = LibraryKey{};
            handle = nullptr;
            alive = false;
        }
    };

    static constexpr std::uint32_t kGenerationBits = 8;
    static constexpr std::uint32_t kIndexBits = 24;
    static constexpr std::uint32_t kGenerationShift = kIndexBits;
    static constexpr std::uint32_t kIndexMask = (1u << kIndexBits) - 1u;
    static constexpr std::uint32_t kGenerationMask = (1u << kGenerationBits) - 1u;
    static constexpr std::size_t kMaxStateCount = static_cast<std::size_t>(kIndexMask);

    void ensureInitialized() const {
        if (!initialized_ || device_ == nullptr) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
                "MPS library manager not initialized");
        }
    }

    void validateKey(const LibraryKey& key) const {
        if (key.identifier.empty()) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
                "Library identifier cannot be empty");
        }
    }

    std::size_t allocateSlot() {
        if (free_list_.empty()) {
            growStatePool(growth_chunk_size_);
            if (free_list_.empty()) {
                ::orteaf::internal::diagnostics::error::throwError(
                    ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
                    "No available MPS library slots");
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
                "Requested MPS library capacity exceeds supported limit");
        }
        const std::size_t start = states_.size();
        states_.reserve(states_.size() + additional);
        free_list_.reserve(free_list_.size() + additional);
        for (std::size_t offset = 0; offset < additional; ++offset) {
            states_.pushBack(State{});
            free_list_.pushBack(start + offset);
        }
    }

    State& ensureAliveState(base::LibraryId id) {
        ensureInitialized();
        const std::size_t index = indexFromId(id);
        if (index >= states_.size()) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
                "MPS library id out of range");
        }
        State& state = states_[index];
        if (!state.alive) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
                "MPS library handle is inactive");
        }
        const std::uint32_t expected_generation = generationFromId(id);
        if ((state.generation & kGenerationMask) != expected_generation) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
                "MPS library handle is stale");
        }
        return state;
    }

    const State& ensureAliveState(base::LibraryId id) const {
        return const_cast<MpsLibraryManager*>(this)->ensureAliveState(id);
    }

    base::LibraryId encodeId(std::size_t index, std::uint32_t generation) const {
        const std::uint32_t encoded_generation = generation & kGenerationMask;
        const std::uint32_t encoded =
            (encoded_generation << kGenerationShift) |
            static_cast<std::uint32_t>(index);
        return base::LibraryId{encoded};
    }

    std::size_t indexFromId(base::LibraryId id) const {
        return static_cast<std::size_t>(static_cast<std::uint32_t>(id) & kIndexMask);
    }

    std::uint32_t generationFromId(base::LibraryId id) const {
        return (static_cast<std::uint32_t>(id) >> kGenerationShift) & kGenerationMask;
    }

    ::orteaf::internal::backend::mps::MPSLibrary_t createLibrary(const LibraryKey& key) {
        switch (key.kind) {
            case LibraryKeyKind::kNamed:
                return BackendOps::createLibraryWithName(device_, key.identifier);
        }
        ::orteaf::internal::diagnostics::error::throwError(
            ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
            "Unsupported MPS library key kind");
    }

    ::orteaf::internal::base::HeapVector<State> states_{};
    ::orteaf::internal::base::HeapVector<std::size_t> free_list_{};
    std::unordered_map<LibraryKey, std::size_t, LibraryKeyHasher> key_to_index_{};
    std::size_t growth_chunk_size_{1};
    bool initialized_{false};
    ::orteaf::internal::backend::mps::MPSDevice_t device_{nullptr};
};

}  // namespace orteaf::internal::runtime::mps

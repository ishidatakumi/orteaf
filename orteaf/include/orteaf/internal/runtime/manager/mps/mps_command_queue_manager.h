#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

#include "orteaf/internal/backend/mps/mps_command_queue.h"
#include "orteaf/internal/backend/mps/mps_event.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/base/strong_id.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/runtime/backend_ops/mps/mps_backend_ops.h"

namespace orteaf::internal::runtime::mps {

/**
 * @brief Stub implementation of an MPS command queue manager.
 *
 * The real implementation will manage a freelist of queues backed by BackendOps.
 * For now we only provide the API surface required by the unit tests so the
 * project builds; behaviour is intentionally minimal.
 */
template <class BackendOps = ::orteaf::internal::runtime::backend_ops::mps::MpsBackendOps>
class MpsCommandQueueManager {
public:
    void initialize(std::size_t capacity) {
        shutdown();

        if (capacity == 0) {
            initialized_ = true;
            return;
        }

        if (capacity > kMaxStateCount) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
                "Requested MPS command queue capacity exceeds supported limit");
        }

        states_.clear();
        free_list_.clear();
        states_.reserve(capacity);
        free_list_.reserve(capacity);

        for (std::size_t index = 0; index < capacity; ++index) {
            State state{};
            state.command_queue = BackendOps::createCommandQueue(nullptr);
            state.event = BackendOps::createEvent(nullptr);
            state.resetHazards();
            state.generation = 0;
            state.in_use = false;
            states_.push_back(std::move(state));
            free_list_.push_back(index);
        }

        initialized_ = true;
    }

    void shutdown() {
        if (states_.empty()) {
            initialized_ = false;
            return;
        }

        for (auto& state : states_) {
            state.destroy();
        }
        states_.clear();
        free_list_.clear();
        initialized_ = false;
    }

    std::size_t capacity() const noexcept { return states_.size(); }

    base::CommandQueueId acquire() {
        ensureInitialized();
        if (free_list_.empty()) {
            growStatePool(states_.empty() ? default_growth_ : states_.size());
            if (free_list_.empty()) {
                ::orteaf::internal::diagnostics::error::throwError(
                    ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
                    "No available MPS command queues");
            }
        }

        const std::size_t index = free_list_.back();
        free_list_.pop_back();
        State& state = states_[index];
        state.in_use = true;
        state.resetHazards();
        return encodeId(index, state.generation);
    }

    void release(base::CommandQueueId id) {
        State& state = ensureActiveState(id);
        state.in_use = false;
        state.resetHazards();
        ++state.generation;
        free_list_.push_back(indexFromId(id));
    }

    ::orteaf::internal::backend::mps::MPSCommandQueue_t getCommandQueue(base::CommandQueueId id) const {
        const State& state = ensureActiveState(id);
        return state.command_queue;
    }

    std::uint64_t submitSerial(base::CommandQueueId id) const {
        const State& state = ensureActiveState(id);
        return state.submit_serial;
    }

    void setSubmitSerial(base::CommandQueueId id, std::uint64_t value) {
        State& state = ensureActiveState(id);
        state.submit_serial = value;
    }

    std::uint64_t completedSerial(base::CommandQueueId id) const {
        const State& state = ensureActiveState(id);
        return state.completed_serial;
    }

    void setCompletedSerial(base::CommandQueueId id, std::uint64_t value) {
        State& state = ensureActiveState(id);
        state.completed_serial = value;
    }

private:
    struct State {
        ::orteaf::internal::backend::mps::MPSCommandQueue_t command_queue{nullptr};
        ::orteaf::internal::backend::mps::MPSEvent_t event{nullptr};
        std::uint64_t submit_serial{0};
        std::uint64_t completed_serial{0};
        std::uint32_t generation{0};
        bool in_use{false};

        void resetHazards() noexcept {
            submit_serial = 0;
            completed_serial = 0;
        }

        void destroy() noexcept {
            if (event != nullptr) {
                BackendOps::destroyEvent(event);
                event = nullptr;
            }
            if (command_queue != nullptr) {
                BackendOps::destroyCommandQueue(command_queue);
                command_queue = nullptr;
            }
            resetHazards();
            in_use = false;
        }
    };

    static constexpr std::uint32_t kGenerationBits = 8;
    static constexpr std::uint32_t kIndexBits = 24;
    static constexpr std::uint32_t kGenerationShift = kIndexBits;
    static constexpr std::uint32_t kIndexMask = (1u << kIndexBits) - 1u;
    static constexpr std::uint32_t kGenerationMask = (1u << kGenerationBits) - 1u;
    static constexpr std::size_t kMaxStateCount = static_cast<std::size_t>(kIndexMask);

    void ensureInitialized() const {
        if (!initialized_) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
                "MPS command queues not initialized");
        }
    }

    base::CommandQueueId encodeId(std::size_t index, std::uint32_t generation) const {
        const std::uint32_t encoded_generation = generation & kGenerationMask;
        const std::uint32_t encoded =
            (encoded_generation << kGenerationShift) |
            static_cast<std::uint32_t>(index);
        return base::CommandQueueId{encoded};
    }

    std::size_t indexFromId(base::CommandQueueId id) const {
        return static_cast<std::size_t>(static_cast<std::uint32_t>(id) & kIndexMask);
    }

    std::uint32_t generationFromId(base::CommandQueueId id) const {
        return (static_cast<std::uint32_t>(id) >> kGenerationShift) & kGenerationMask;
    }

    State& ensureActiveState(base::CommandQueueId id) {
        ensureInitialized();
        const std::size_t index = indexFromId(id);
        if (index >= states_.size()) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
                "MPS command queue id out of range");
        }
        State& state = states_[index];
        if (!state.in_use) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
                "MPS command queue is inactive");
        }
        const std::uint32_t expected_generation = generationFromId(id);
        if ((state.generation & kGenerationMask) != expected_generation) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
                "MPS command queue handle is stale");
        }
        return state;
    }

    const State& ensureActiveState(base::CommandQueueId id) const {
        return const_cast<MpsCommandQueueManager*>(this)->ensureActiveState(id);
    }

    void growStatePool(std::size_t additional_count) {
        if (additional_count == 0) {
            return;
        }
        if (states_.size() + additional_count > kMaxStateCount) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
                "Requested MPS command queue capacity exceeds supported limit");
        }
        const std::size_t start_index = states_.size();
        states_.reserve(states_.size() + additional_count);
        free_list_.reserve(states_.size() + additional_count);

        for (std::size_t i = 0; i < additional_count; ++i) {
            State state{};
            state.command_queue = BackendOps::createCommandQueue(nullptr);
            state.event = BackendOps::createEvent(nullptr);
            state.resetHazards();
            state.generation = 0;
            state.in_use = false;
            states_.push_back(std::move(state));
            free_list_.push_back(start_index + i);
        }
    }

    ::orteaf::internal::base::HeapVector<State> states_;
    ::orteaf::internal::base::HeapVector<std::size_t> free_list_;
    const std::size_t default_growth_{4};
    bool initialized_{false};
};

inline MpsCommandQueueManager<> MpsCommandQueueManagerInstance{};

inline MpsCommandQueueManager<>& GetMpsCommandQueueManager() {
    return MpsCommandQueueManagerInstance;
}

}  // namespace orteaf::internal::runtime::mps

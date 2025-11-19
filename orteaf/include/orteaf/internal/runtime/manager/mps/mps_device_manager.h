#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

#include "orteaf/internal/architecture/architecture.h"
#include "orteaf/internal/backend/mps/mps_device.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/base/strong_id.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/runtime/backend_ops/mps/mps_backend_ops.h"
#include "orteaf/internal/runtime/backend_ops/mps/mps_backend_ops_concepts.h"
#include "orteaf/internal/runtime/manager/mps/mps_command_queue_manager.h"
#include "orteaf/internal/runtime/manager/mps/mps_heap_manager.h"
#include "orteaf/internal/runtime/manager/mps/mps_library_manager.h"

namespace orteaf::internal::runtime::mps {

template <class BackendOps = ::orteaf::internal::runtime::backend_ops::mps::MpsBackendOps>
requires ::orteaf::internal::runtime::backend_ops::mps::MpsRuntimeBackendOps<BackendOps>
class MpsDeviceManager {
public:
    void setCommandQueueInitialCapacity(std::size_t capacity) {
        command_queue_initial_capacity_ = capacity;
    }

    std::size_t commandQueueInitialCapacity() const noexcept {
        return command_queue_initial_capacity_;
    }

    void setHeapInitialCapacity(std::size_t capacity) {
        heap_initial_capacity_ = capacity;
    }

    std::size_t heapInitialCapacity() const noexcept {
        return heap_initial_capacity_;
    }

    void setLibraryInitialCapacity(std::size_t capacity) {
        library_initial_capacity_ = capacity;
    }

    std::size_t libraryInitialCapacity() const noexcept {
        return library_initial_capacity_;
    }

    void initialize() {
        shutdown();

        const int device_count = BackendOps::getDeviceCount();
        if (device_count <= 0) {
            initialized_ = true;
            return;
        }

        states_.resize(static_cast<std::size_t>(device_count));

        for (int i = 0; i < device_count; ++i) {
            auto& state = states_[i];
            state.reset();

            const auto device = BackendOps::getDevice(static_cast<::orteaf::internal::backend::mps::MPSInt_t>(i));
            state.device = device;
            state.is_alive = device != nullptr;

            const ::orteaf::internal::base::DeviceId device_id{static_cast<std::uint32_t>(i)};
            state.arch = state.is_alive
                ? BackendOps::detectArchitecture(device_id)
                : ::orteaf::internal::architecture::Architecture::mps_generic;
            if (state.is_alive) {
                state.command_queue_manager.initialize(device, command_queue_initial_capacity_);
                state.heap_manager.initialize(device, heap_initial_capacity_);
                state.library_manager.initialize(device, library_initial_capacity_);
            } else {
                state.command_queue_manager.shutdown();
                state.heap_manager.shutdown();
                state.library_manager.shutdown();
            }
        }

        initialized_ = true;
    }

    void shutdown() {
        if (states_.empty()) {
            initialized_ = false;
            return;
        }
        for (std::size_t i = 0; i < states_.size(); ++i) {
            states_[i].reset();
        }
        states_.clear();
        initialized_ = false;
    }

    std::size_t getDeviceCount() const {
        return states_.size();
    }

    ::orteaf::internal::backend::mps::MPSDevice_t getDevice(::orteaf::internal::base::DeviceId id) const {
        const State& state = ensureValid(id);
        return state.device;
    }

    ::orteaf::internal::architecture::Architecture getArch(::orteaf::internal::base::DeviceId id) const {
        const State& state = ensureValid(id);
        return state.arch;
    }

    ::orteaf::internal::runtime::mps::MpsCommandQueueManager<BackendOps>& commandQueueManager(
        ::orteaf::internal::base::DeviceId id) {
        State& state = ensureValidState(id);
        return state.command_queue_manager;
    }

    const ::orteaf::internal::runtime::mps::MpsCommandQueueManager<BackendOps>& commandQueueManager(
        ::orteaf::internal::base::DeviceId id) const {
        const State& state = ensureValid(id);
        return state.command_queue_manager;
    }

    ::orteaf::internal::runtime::mps::MpsHeapManager<BackendOps>& heapManager(
        ::orteaf::internal::base::DeviceId id) {
        State& state = ensureValidState(id);
        return state.heap_manager;
    }

    const ::orteaf::internal::runtime::mps::MpsHeapManager<BackendOps>& heapManager(
        ::orteaf::internal::base::DeviceId id) const {
        const State& state = ensureValid(id);
        return state.heap_manager;
    }

    ::orteaf::internal::runtime::mps::MpsLibraryManager<BackendOps>& libraryManager(
        ::orteaf::internal::base::DeviceId id) {
        State& state = ensureValidState(id);
        return state.library_manager;
    }

    const ::orteaf::internal::runtime::mps::MpsLibraryManager<BackendOps>& libraryManager(
        ::orteaf::internal::base::DeviceId id) const {
        const State& state = ensureValid(id);
        return state.library_manager;
    }

    bool isAlive(::orteaf::internal::base::DeviceId id) const {
        const std::size_t index = static_cast<std::size_t>(static_cast<std::uint32_t>(id));
        if (index >= states_.size()) {
            return false;
        }
        return states_[index].is_alive;
    }

#if ORTEAF_ENABLE_TEST
    struct DebugState {
        std::size_t device_count{0};
        bool initialized{false};
    };

    struct DeviceDebugState {
        bool in_range{false};
        bool is_alive{false};
        bool has_device{false};
        ::orteaf::internal::architecture::Architecture arch{
            ::orteaf::internal::architecture::Architecture::mps_generic};
    };

    DebugState debugState() const {
        return DebugState{states_.size(), initialized_};
    }

    DeviceDebugState debugState(::orteaf::internal::base::DeviceId id) const {
        DeviceDebugState snapshot{};
        if (!initialized_) {
            return snapshot;
        }
        const std::size_t index = static_cast<std::size_t>(static_cast<std::uint32_t>(id));
        if (index >= states_.size()) {
            return snapshot;
        }
        const State& state = states_[index];
        snapshot.in_range = true;
        snapshot.is_alive = state.is_alive;
        snapshot.has_device = state.device != nullptr;
        snapshot.arch = state.arch;
        return snapshot;
    }
#endif

private:
    struct State {
        ::orteaf::internal::backend::mps::MPSDevice_t device{nullptr};
        ::orteaf::internal::architecture::Architecture arch{
            ::orteaf::internal::architecture::Architecture::mps_generic};
        bool is_alive{false};
        ::orteaf::internal::runtime::mps::MpsCommandQueueManager<BackendOps> command_queue_manager{};
        ::orteaf::internal::runtime::mps::MpsHeapManager<BackendOps> heap_manager{};
        ::orteaf::internal::runtime::mps::MpsLibraryManager<BackendOps> library_manager{};

        State() = default;
        State(const State&) = delete;
        State& operator=(const State&) = delete;

        State(State&& other) noexcept {
            moveFrom(std::move(other));
        }

        State& operator=(State&& other) noexcept {
            if (this != &other) {
                reset();
                moveFrom(std::move(other));
            }
            return *this;
        }

        ~State() {
            reset();
        }

        void reset() noexcept {
            command_queue_manager.shutdown();
            heap_manager.shutdown();
            library_manager.shutdown();
            if (device != nullptr) {
                BackendOps::releaseDevice(device);
            }
            device = nullptr;
            arch = ::orteaf::internal::architecture::Architecture::mps_generic;
            is_alive = false;
        }

    private:
        void moveFrom(State&& other) noexcept {
            command_queue_manager = std::move(other.command_queue_manager);
            heap_manager = std::move(other.heap_manager);
            library_manager = std::move(other.library_manager);
            device = other.device;
            arch = other.arch;
            is_alive = other.is_alive;
            other.device = nullptr;
            other.is_alive = false;
        }
    };

    const State& ensureValid(::orteaf::internal::base::DeviceId id) const {
        if (!initialized_) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
                "MPS devices not initialized");
        }
        const std::size_t index = static_cast<std::size_t>(static_cast<std::uint32_t>(id));
        if (index >= states_.size()) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
                "MPS device id out of range");
        }
        const State& state = states_[index];
        if (!state.is_alive || state.device == nullptr) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
                "MPS device is unavailable");
        }
        return state;
    }

    State& ensureValidState(::orteaf::internal::base::DeviceId id) {
        return const_cast<State&>(ensureValid(id));
    }

    ::orteaf::internal::base::HeapVector<State> states_;
    bool initialized_{false};
    std::size_t command_queue_initial_capacity_{0};
    std::size_t heap_initial_capacity_{0};
    std::size_t library_initial_capacity_{0};
};

inline MpsDeviceManager<>& GetMpsDeviceManager() {
    static MpsDeviceManager<> instance{};
    return instance;
}

} // namespace orteaf::internal::runtime::mps

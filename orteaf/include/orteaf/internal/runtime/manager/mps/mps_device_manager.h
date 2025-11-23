#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>
#include <utility>

#include "orteaf/internal/architecture/architecture.h"
#include "orteaf/internal/backend/mps/wrapper/mps_device.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/base/strong_id.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/backend/mps/mps_slow_ops.h"
#include "orteaf/internal/runtime/manager/mps/mps_command_queue_manager.h"
#include "orteaf/internal/runtime/manager/mps/mps_heap_manager.h"
#include "orteaf/internal/runtime/manager/mps/mps_library_manager.h"

namespace orteaf::internal::runtime::mps {

class MpsDeviceManager {
public:
    using BackendOps = ::orteaf::internal::runtime::backend_ops::mps::MpsSlowOps;

    MpsDeviceManager() = default;
    MpsDeviceManager(const MpsDeviceManager&) = delete;
    MpsDeviceManager& operator=(const MpsDeviceManager&) = delete;
    MpsDeviceManager(MpsDeviceManager&&) = default;
    MpsDeviceManager& operator=(MpsDeviceManager&&) = default;
    ~MpsDeviceManager() = default;

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

    void initialize(BackendOps *ops);

    void shutdown();

    std::size_t getDeviceCount() const {
        return states_.size();
    }

    ::orteaf::internal::backend::mps::MPSDevice_t getDevice(::orteaf::internal::base::DeviceId id) const;

    ::orteaf::internal::architecture::Architecture getArch(::orteaf::internal::base::DeviceId id) const;

    ::orteaf::internal::runtime::mps::MpsCommandQueueManager& commandQueueManager(
        ::orteaf::internal::base::DeviceId id);

    const ::orteaf::internal::runtime::mps::MpsCommandQueueManager& commandQueueManager(
        ::orteaf::internal::base::DeviceId id) const;

    ::orteaf::internal::runtime::mps::MpsHeapManager& heapManager(
        ::orteaf::internal::base::DeviceId id);

    const ::orteaf::internal::runtime::mps::MpsHeapManager& heapManager(
        ::orteaf::internal::base::DeviceId id) const;

    ::orteaf::internal::runtime::mps::MpsLibraryManager& libraryManager(
        ::orteaf::internal::base::DeviceId id);

    const ::orteaf::internal::runtime::mps::MpsLibraryManager& libraryManager(
        ::orteaf::internal::base::DeviceId id) const;

    bool isAlive(::orteaf::internal::base::DeviceId id) const;

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
            ::orteaf::internal::architecture::Architecture::MpsGeneric};
    };

    DebugState debugState() const {
        return DebugState{states_.size(), initialized_};
    }

    DeviceDebugState debugState(::orteaf::internal::base::DeviceId id) const;
#endif

private:
    struct State {
        ::orteaf::internal::backend::mps::MPSDevice_t device{nullptr};
        ::orteaf::internal::architecture::Architecture arch{
            ::orteaf::internal::architecture::Architecture::MpsGeneric};
        bool is_alive{false};
        ::orteaf::internal::runtime::mps::MpsCommandQueueManager command_queue_manager{};
        ::orteaf::internal::runtime::mps::MpsHeapManager heap_manager{};
        ::orteaf::internal::runtime::mps::MpsLibraryManager library_manager{};

        State() = default;
        State(const State&) = delete;
        State& operator=(const State&) = delete;

        State(State&& other) noexcept {
            moveFrom(std::move(other));
        }

        State& operator=(State&& other) noexcept {
            if (this != &other) {
                reset(nullptr);
                moveFrom(std::move(other));
            }
            return *this;
        }

        ~State() {
            reset(nullptr);
        }

        void reset(BackendOps *ops) noexcept;

    private:
        void moveFrom(State&& other) noexcept;
    };

    const State& ensureValid(::orteaf::internal::base::DeviceId id) const;

    State& ensureValidState(::orteaf::internal::base::DeviceId id) {
        return const_cast<State&>(ensureValid(id));
    }

    ::orteaf::internal::base::HeapVector<State> states_;
    bool initialized_{false};
    std::size_t command_queue_initial_capacity_{0};
    std::size_t heap_initial_capacity_{0};
    std::size_t library_initial_capacity_{0};
    BackendOps *ops_{nullptr};
};

inline MpsDeviceManager& GetMpsDeviceManager() {
    static MpsDeviceManager instance{};
    return instance;
}

} // namespace orteaf::internal::runtime::mps

#endif // ORTEAF_ENABLE_MPS

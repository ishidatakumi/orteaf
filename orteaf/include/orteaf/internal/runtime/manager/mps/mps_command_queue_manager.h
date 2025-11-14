#pragma once

#include <cstddef>
#include <cstdint>

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
        (void)capacity;
        shutdown();
        initialized_ = true;
        capacity_ = capacity;
    }

    void shutdown() {
        initialized_ = false;
    }

    std::size_t capacity() const noexcept { return 0; }

    base::CommandQueueId acquire() {
        ensureInitialized();
        ::orteaf::internal::diagnostics::error::throwError(
            ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
            "MPS command queues not implemented");
    }

    void release(base::CommandQueueId) {
        ensureInitialized();
        ::orteaf::internal::diagnostics::error::throwError(
            ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
            "MPS command queues not implemented");
    }

    ::orteaf::internal::backend::mps::MPSCommandQueue_t getCommandQueue(base::CommandQueueId) const {
        ensureInitialized();
        ::orteaf::internal::diagnostics::error::throwError(
            ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
            "MPS command queues not implemented");
    }

    std::uint64_t submitSerial(base::CommandQueueId) const {
        ensureInitialized();
        return 0;
    }

    void setSubmitSerial(base::CommandQueueId, std::uint64_t) {
        ensureInitialized();
    }

    std::uint64_t completedSerial(base::CommandQueueId) const {
        ensureInitialized();
        return 0;
    }

    void setCompletedSerial(base::CommandQueueId, std::uint64_t) {
        ensureInitialized();
    }

private:
    void ensureInitialized() const {
        if (!initialized_) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
                "MPS command queues not initialized");
        }
    }

private:
    struct State {
        std::uint64_t submit_serial{0};
        std::uint64_t completed_serial{0};
    };

    bool initialized_{false};
    std::size_t capacity_{0};
};

inline MpsCommandQueueManager<> MpsCommandQueueManagerInstance{};

inline MpsCommandQueueManager<>& GetMpsCommandQueueManager() {
    return MpsCommandQueueManagerInstance;
}

}  // namespace orteaf::internal::runtime::mps

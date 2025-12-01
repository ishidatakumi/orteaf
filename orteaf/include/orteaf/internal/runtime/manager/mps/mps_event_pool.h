#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>

#include "orteaf/internal/backend/mps/wrapper/mps_event.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/base/lease.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/backend/mps/mps_slow_ops.h"

namespace orteaf::internal::runtime::mps {

class MpsEventPool {
public:
    using BackendOps = ::orteaf::internal::runtime::backend_ops::mps::MpsSlowOps;
    using Event = ::orteaf::internal::backend::mps::MPSEvent_t;
    using EventLease = ::orteaf::internal::base::Lease<void, Event, MpsEventPool>;

    MpsEventPool() = default;
    MpsEventPool(const MpsEventPool&) = delete;
    MpsEventPool& operator=(const MpsEventPool&) = delete;
    MpsEventPool(MpsEventPool&&) = default;
    MpsEventPool& operator=(MpsEventPool&&) = default;
    ~MpsEventPool() = default;

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
                    BackendOps *ops,
                    std::size_t initial_capacity = 0);

    void shutdown();

    EventLease acquireEvent();

    // Manual release helper for callers wanting explicit return.
    void release(EventLease& lease) noexcept {
        if (lease) {
            release(lease.get());
        }
    }

    std::size_t availableCount() const noexcept { return free_list_.size(); }

    std::size_t inUseCount() const noexcept { return active_count_; }

#if ORTEAF_ENABLE_TEST
    struct DebugState {
        std::size_t growth_chunk_size{0};
        std::size_t available_count{0};
        std::size_t in_use_count{0};
        std::size_t total_created{0};
    };

    DebugState debugState() const noexcept {
        DebugState snapshot{};
        snapshot.growth_chunk_size = growth_chunk_size_;
        snapshot.available_count = free_list_.size();
        snapshot.in_use_count = active_count_;
        snapshot.total_created = total_created_;
        return snapshot;
    }
#endif

private:
    void ensureInitialized() const;

    void growFreeList(std::size_t count);

    void release(Event handle) noexcept;

    std::size_t growth_chunk_size_{1};
    bool initialized_{false};
    ::orteaf::internal::backend::mps::MPSDevice_t device_{nullptr};
    base::HeapVector<Event> free_list_{};
    std::size_t active_count_{0};
    BackendOps *ops_{nullptr};
#if ORTEAF_ENABLE_TEST
    std::size_t total_created_{0};
#endif
};

}  // namespace orteaf::internal::runtime::mps

#endif // ORTEAF_ENABLE_MPS

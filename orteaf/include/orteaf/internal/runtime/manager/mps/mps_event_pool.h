#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>

#include "orteaf/internal/backend/mps/mps_event.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/runtime/backend_ops/mps/mps_slow_ops.h"

namespace orteaf::internal::runtime::mps {

class MpsEventPool {
public:
    using BackendOps = ::orteaf::internal::runtime::backend_ops::mps::MpsSlowOps;
    using EventHandle = ::orteaf::internal::backend::mps::MPSEvent_t;

    class Handle {
    public:
        Handle() = delete;
        Handle(const Handle&) = delete;
        Handle& operator=(const Handle&) = delete;

        Handle(Handle&& other) noexcept { moveFrom(other); }
        Handle& operator=(Handle&& other) noexcept {
            if (this != &other) {
                release();
                moveFrom(other);
            }
            return *this;
        }

        ~Handle() { release(); }

        EventHandle get() const {
            if (!active_) {
                ::orteaf::internal::diagnostics::error::throwError(
                    ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
                    "Event handle is not active");
            }
            return handle_;
        }

        explicit operator bool() const noexcept { return active_; }

        void release() {
            if (active_ && pool_ != nullptr) {
                pool_->release(handle_);
            }
            pool_ = nullptr;
            handle_ = nullptr;
            active_ = false;
        }

    private:
        friend class MpsEventPool;

        Handle(MpsEventPool* pool, EventHandle handle) noexcept
            : pool_(pool), handle_(handle), active_(true) {}

        void moveFrom(Handle& other) noexcept {
            pool_ = other.pool_;
            handle_ = other.handle_;
            active_ = other.active_;
            other.pool_ = nullptr;
            other.handle_ = nullptr;
            other.active_ = false;
        }

        MpsEventPool* pool_{nullptr};
        EventHandle handle_{nullptr};
        bool active_{false};
    };

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

    Handle acquireEvent();

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
    // Pool instances must remain immobile while handles exist.
    MpsEventPool(const MpsEventPool&) = delete;
    MpsEventPool& operator=(const MpsEventPool&) = delete;
    MpsEventPool(MpsEventPool&&) = delete;
    MpsEventPool& operator=(MpsEventPool&&) = delete;

    void ensureInitialized() const;

    void growFreeList(std::size_t count);

    void release(EventHandle handle);

    std::size_t growth_chunk_size_{1};
    bool initialized_{false};
    ::orteaf::internal::backend::mps::MPSDevice_t device_{nullptr};
    base::HeapVector<EventHandle> free_list_{};
    std::size_t active_count_{0};
    BackendOps *ops_{nullptr};
#if ORTEAF_ENABLE_TEST
    std::size_t total_created_{0};
#endif
};

}  // namespace orteaf::internal::runtime::mps

#endif // ORTEAF_ENABLE_MPS

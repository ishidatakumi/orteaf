#pragma once

#include <cstddef>
#include <unordered_set>

#include "orteaf/internal/backend/mps/mps_fence.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/runtime/backend_ops/mps/mps_backend_ops.h"
#include "orteaf/internal/runtime/backend_ops/mps/mps_backend_ops_concepts.h"

namespace orteaf::internal::runtime::mps {

template <class BackendOps = ::orteaf::internal::runtime::backend_ops::mps::MpsBackendOps>
requires ::orteaf::internal::runtime::backend_ops::mps::MpsRuntimeBackendOps<BackendOps>
class MpsFencePool {
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
                    std::size_t initial_capacity = 0) {
        if (device == nullptr) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
                "MPS fence pool requires a valid device");
        }
        if (initialized_) {
            shutdown();
        }
        device_ = device;
        initialized_ = true;
        #if ORTEAF_ENABLE_TEST
        total_created_ = 0;
        #endif
        free_list_.clear();
        free_list_.reserve(initial_capacity);
        if (initial_capacity > 0) {
            growFreeList(initial_capacity);
        }
    }

    void shutdown() {
        if (!initialized_) {
            free_list_.clear();
            active_handles_.clear();
            device_ = nullptr;
            total_created_ = 0;
            return;
        }
        if (!active_handles_.empty()) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
                "Cannot shutdown MPS fence pool while fences are in use");
        }
        for (std::size_t i = 0; i < free_list_.size(); ++i) {
            BackendOps::destroyFence(free_list_[i]);
        }
        free_list_.clear();
        active_handles_.clear();
        device_ = nullptr;
        #if ORTEAF_ENABLE_TEST
        total_created_ = 0;
        #endif
        initialized_ = false;
    }

    ::orteaf::internal::backend::mps::MPSFence_t acquireFence() {
        ensureInitialized();
        if (free_list_.empty()) {
            growFreeList(growth_chunk_size_);
        }
        auto handle = free_list_.back();
        free_list_.resize(free_list_.size() - 1);
        active_handles_.insert(handle);
        return handle;
    }

    void releaseFence(::orteaf::internal::backend::mps::MPSFence_t fence) {
        ensureInitialized();
        if (fence == nullptr) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
                "Cannot release null fence to MPS fence pool");
        }
        const auto erased = active_handles_.erase(fence);
        if (erased == 0) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
                "Fence handle does not belong to this pool or is already released");
        }
        free_list_.pushBack(fence);
    }

    std::size_t availableCount() const noexcept { return free_list_.size(); }

    std::size_t inUseCount() const noexcept { return active_handles_.size(); }

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
        snapshot.in_use_count = active_handles_.size();
        snapshot.total_created = total_created_;
        return snapshot;
    }
#endif

private:
    void ensureInitialized() const {
        if (!initialized_) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
                "MPS fence pool has not been initialized");
        }
    }

    void growFreeList(std::size_t count) {
        for (std::size_t i = 0; i < count; ++i) {
            auto handle = BackendOps::createFence(device_);
            if (handle == nullptr) {
                ::orteaf::internal::diagnostics::error::throwError(
                    ::orteaf::internal::diagnostics::error::OrteafErrc::OperationFailed,
                    "Backend failed to create MPS fence");
            }
            free_list_.pushBack(handle);
            #if ORTEAF_ENABLE_TEST
            ++total_created_;
            #endif
        }
    }

    std::size_t growth_chunk_size_{1};
    bool initialized_{false};
    ::orteaf::internal::backend::mps::MPSDevice_t device_{nullptr};
    base::HeapVector<::orteaf::internal::backend::mps::MPSFence_t> free_list_{};
    std::unordered_set<::orteaf::internal::backend::mps::MPSFence_t> active_handles_{};
    #if ORTEAF_ENABLE_TEST
    std::size_t total_created_{0};
    #endif
};

}  // namespace orteaf::internal::runtime::mps

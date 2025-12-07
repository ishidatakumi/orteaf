#pragma once

#include <cstddef>
#include <string>
#include <utility>

#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/base/lease.h"
#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::runtime::base {

template <typename Derived, typename Traits>
class ResourcePool {
public:
    using Resource = typename Traits::ResourceType;
    using Device = typename Traits::DeviceType;
    using Ops = typename Traits::OpsType;
    using PoolLease = ::orteaf::internal::base::Lease<void, Resource, ResourcePool>;

    ResourcePool() = default;
    ResourcePool(const ResourcePool&) = delete;
    ResourcePool& operator=(const ResourcePool&) = delete;
    ResourcePool(ResourcePool&&) = default;
    ResourcePool& operator=(ResourcePool&&) = default;
    ~ResourcePool() = default;

    void setGrowthChunkSize(std::size_t chunk) {
        if (chunk == 0) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
                "Growth chunk size must be > 0");
        }
        growth_chunk_size_ = chunk;
    }

    std::size_t growthChunkSize() const noexcept { return growth_chunk_size_; }

    void initialize(Device device, Ops* ops, std::size_t initial_capacity = 0) {
        if (device == nullptr) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
                std::string(Traits::Name) + " requires a valid device");
        }
        if (ops == nullptr) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
                std::string(Traits::Name) + " requires valid ops");
        }
        if (initialized_) {
            shutdown();
        }
        device_ = device;
        ops_ = ops;
#if ORTEAF_ENABLE_TEST
        total_created_ = 0;
#endif
        free_list_.clear();
        free_list_.reserve(initial_capacity);
        active_count_ = 0;
        if (initial_capacity > 0) {
            growFreeList(initial_capacity);
        }
        initialized_ = true;
    }

    void shutdown() {
        if (!initialized_) {
            free_list_.clear();
            active_count_ = 0;
            device_ = nullptr;
            ops_ = nullptr;
#if ORTEAF_ENABLE_TEST
            total_created_ = 0;
#endif
            return;
        }
        if (active_count_ != 0) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
                std::string("Cannot shutdown ") + Traits::Name + " while resources are in use");
        }
        for (std::size_t i = 0; i < free_list_.size(); ++i) {
            Traits::destroy(ops_, free_list_[i]);
        }
        free_list_.clear();
        active_count_ = 0;
        device_ = nullptr;
        ops_ = nullptr;
#if ORTEAF_ENABLE_TEST
        total_created_ = 0;
#endif
        initialized_ = false;
    }

    PoolLease acquire() {
        ensureInitialized();
        if (free_list_.empty()) {
            growFreeList(growth_chunk_size_);
        }
        auto handle = free_list_.back();
        free_list_.resize(free_list_.size() - 1);
        ++active_count_;
        return PoolLease{this, handle};
    }

    void release(PoolLease& lease) noexcept {
        if (lease) {
            release(lease.getForManager());
            lease.invalidate();
        }
    }

    void release(Resource handle) noexcept {
        if (!initialized_ || handle == nullptr) {
            return;
        }
        if (active_count_ == 0) {
            return;
        }
        free_list_.pushBack(handle);
        --active_count_;
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
    void ensureInitialized() const {
        if (!initialized_) {
            ::orteaf::internal::diagnostics::error::throwError(
                ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
                std::string(Traits::Name) + " has not been initialized");
        }
    }

    void growFreeList(std::size_t count) {
        for (std::size_t i = 0; i < count; ++i) {
            auto handle = Traits::create(ops_, device_);
            if (handle == nullptr) {
                ::orteaf::internal::diagnostics::error::throwError(
                    ::orteaf::internal::diagnostics::error::OrteafErrc::OperationFailed,
                    "Backend failed to create " + std::string(Traits::Name) + " resource");
            }
            free_list_.pushBack(handle);
#if ORTEAF_ENABLE_TEST
            ++total_created_;
#endif
        }
    }

    std::size_t growth_chunk_size_{1};
    bool initialized_{false};
    Device device_{nullptr};
    ::orteaf::internal::base::HeapVector<Resource> free_list_{};
    std::size_t active_count_{0};
    Ops* ops_{nullptr};
#if ORTEAF_ENABLE_TEST
    std::size_t total_created_{0};
#endif
};

}  // namespace orteaf::internal::runtime::base

#pragma once

#include <memory>
#include <utility>
#include <type_traits>

#include "orteaf/internal/diagnostics/error/error_macros.h"

namespace orteaf::internal::base {

/**
 * @brief RAII handle wrapper that pairs a Handle with a cached resource value.
 *
 * Construction is restricted to the Manager type (friend). Managers call the
 * private ctor with the acquired resource; destruction releases via Manager::release.
 */
template <class HandleT, class ResourceT, class ManagerT>
class Lease {
    friend ManagerT;

public:
    Lease(const Lease&) = delete;
    Lease& operator=(const Lease&) = delete;

    Lease(Lease&& other) noexcept
        : manager_(std::exchange(other.manager_, nullptr)),
          handle_(std::move(other.handle_)),
          resource_(std::move(other.resource_)) {}

    Lease& operator=(Lease&& other) noexcept {
        if (this != &other) {
            release();
            manager_ = std::exchange(other.manager_, nullptr);
            handle_ = std::move(other.handle_);
            resource_ = std::move(other.resource_);
        }
        return *this;
    }

    ~Lease() noexcept { release(); }

    const HandleT& handle() const noexcept { return handle_; }

    ResourceT& get() {
        ensureValid();
        return resource_;
    }
    const ResourceT& get() const {
        ensureValid();
        return resource_;
    }

    ResourceT* operator->() noexcept { return std::addressof(resource_); }
    const ResourceT* operator->() const noexcept { return std::addressof(resource_); }

    explicit operator bool() const noexcept { return manager_ != nullptr; }

    // Explicitly release early; safe to call multiple times. Never throws.
    void release() noexcept { doRelease(); }

private:
    void ensureValid() const noexcept {}

    Lease(ManagerT* mgr, HandleT handle, ResourceT resource) noexcept
        : manager_(mgr), handle_(std::move(handle)), resource_(std::move(resource)) {}

    void doRelease() noexcept {
        if (manager_) {
            manager_->release(*this);
            manager_ = nullptr;
        }
    }

    ManagerT* manager_{nullptr};
    HandleT handle_{};
    ResourceT resource_{};
};

/**
 * @brief Handle を持たないリソース向け特殊化。
 *
 * Manager::release は ResourceT のみで解放できることを前提とする。
 */
template <class ResourceT, class ManagerT>
class Lease<void, ResourceT, ManagerT> {
    friend ManagerT;

public:
    Lease(const Lease&) = delete;
    Lease& operator=(const Lease&) = delete;

    Lease(Lease&& other) noexcept
        : manager_(std::exchange(other.manager_, nullptr)),
          resource_(std::move(other.resource_)) {}

    Lease& operator=(Lease&& other) noexcept {
        if (this != &other) {
            release();
            manager_ = std::exchange(other.manager_, nullptr);
            resource_ = std::move(other.resource_);
        }
        return *this;
    }

    ~Lease() noexcept { release(); }

    ResourceT& get() {
        ensureValid();
        return resource_;
    }
    const ResourceT& get() const {
        ensureValid();
        return resource_;
    }

    ResourceT* operator->() noexcept { return std::addressof(resource_); }
    const ResourceT* operator->() const noexcept { return std::addressof(resource_); }

    explicit operator bool() const noexcept { return manager_ != nullptr; }

    // Explicitly release early; safe to call multiple times. Never throws.
    void release() noexcept { doRelease(); }

private:
    void ensureValid() const noexcept {}

    Lease(ManagerT* mgr, ResourceT resource) noexcept
        : manager_(mgr), resource_(std::move(resource)) {}

    void doRelease() noexcept {
        if (manager_) {
            manager_->release(*this);
            manager_ = nullptr;
        }
    }

    ManagerT* manager_{nullptr};
    ResourceT resource_{};
};

}  // namespace orteaf::internal::base

#pragma once

#include <atomic>
#include <type_traits>
#include <utility>

namespace orteaf::internal::base {
/**
 * @brief A generic shared lease implementation that manages a resource using
 * reference counting provided by the ManagerT.
 *
 * This class is copyable and movable. Copying increments the reference count
 * via `manager_->retain(handle_)`. Destruction or release decrements the
 * reference count via `manager_->release(handle_)`.
 *
 * @tparam HandleT The type of the handle used to identify the resource in the
 * manager.
 * @tparam ResourceT The type of the actual resource being managed.
 * @tparam ManagerT The type of the manager that owns the resource. Must provide
 * `retain(HandleT)` and `release(HandleT)`.
 */
template <class HandleT, class ResourceT, class ManagerT> 
class SharedLease {
public:
  using HandleType = HandleT;
  using ResourceType = ResourceT;
  using ManagerType = ManagerT;

  SharedLease() = default;
  SharedLease(const SharedLease &other) { copyFrom(other); }

  SharedLease &operator=(const SharedLease &other) {
    if (this != &other) {
      release();
      copyFrom(other);
    }
    return *this;
  }

  SharedLease(SharedLease&& other) noexcept
        : manager_(std::exchange(other.manager_, nullptr)),
          handle_(std::move(other.handle_)),
          resource_(std::move(other.resource_)) {}

  SharedLease &operator=(SharedLease &&other) noexcept {
    if (this != &other) {
      release();
      manager_ = std::exchange(other.manager_, nullptr);
      handle_ = std::move(other.handle_);
      resource_ = std::move(other.resource_);
    }
    return *this;
  }

  ~SharedLease() { release(); }

  
    auto operator->() noexcept {
        if constexpr (std::is_pointer_v<ResourceT>) {
            return resource_;
        } else {
            return std::addressof(resource_);
        }
    }
    auto operator->() const noexcept {
        if constexpr (std::is_pointer_v<ResourceT>) {
            return resource_;
        } else {
            return std::addressof(resource_);
        }
    }

    decltype(auto) operator*() noexcept {
        if constexpr (std::is_pointer_v<ResourceT>) {
            return *resource_;
        } else {
            return (resource_);
        }
    }
    decltype(auto) operator*() const noexcept {
        if constexpr (std::is_pointer_v<ResourceT>) {
            return *resource_;
        } else {
            return (resource_);
        }
    }

    // Pointer access helper (returns raw pointer regardless of ResourceT being pointer or object).
    auto pointer() noexcept {
        if constexpr (std::is_pointer_v<ResourceT>) {
            return resource_;
        } else {
            return std::addressof(resource_);
        }
    }
    auto pointer() const noexcept {
        if constexpr (std::is_pointer_v<ResourceT>) {
            return resource_;
        } else {
            return std::addressof(resource_);
        }
    }

    template <class F>
    decltype(auto) with_resource(F&& f) {
        return static_cast<F&&>(f)(resource_);
    }

    template <class F>
    decltype(auto) with_resource(F&& f) const {
        return static_cast<F&&>(f)(resource_);
    }

  explicit operator bool() const { return manager_ != nullptr; }

  HandleType &handle() { return handle_; }

  void release() {
    doRelease();
  }

private:
  friend ManagerType;

  SharedLease(ManagerType *manager, HandleType handle, ResourceType resource) 
  : manager_(manager), handle_(handle), resource_(resource) {}

  void copyFrom(const SharedLease &other) {
    *this = other.manager_->acquire(other.handle_);
  }

  void doRelease() {
    if (manager_) {
      manager_->release(*this);
      manager_ = nullptr;
    }
  }

  void invalidate() {
    manager_ = nullptr;
    handle_ = HandleType{};
    resource_ = ResourceType{};
  }

  HandleType handle_;
  ManagerType *manager_;
  ResourceType resource_;
};

} // namespace orteaf::internal::base

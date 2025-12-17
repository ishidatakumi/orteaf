#pragma once

#include <memory>
#include <type_traits>
#include <utility>

#include <orteaf/internal/runtime/base/lease/category.h>

namespace orteaf::internal::runtime::base {

/// @brief Shared lease - shared ownership with reference counting
/// @details Multiple leases can share the same resource. Similar to
/// shared_ptr. The manager tracks reference counts via the control block.
template <class HandleT, class ResourceT, class ManagerT> class SharedLease {
  friend ManagerT;

public:
  using HandleType = HandleT;
  using ResourceType = ResourceT;
  using ManagerType = ManagerT;
  using CompatibleCategory = lease_category::Shared;

  SharedLease() = default;

  SharedLease(const SharedLease &other) { copyFrom(other); }

  SharedLease &operator=(const SharedLease &other) {
    if (this != &other) {
      release();
      copyFrom(other);
    }
    return *this;
  }

  SharedLease(SharedLease &&other) noexcept
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

  const HandleT &handle() const noexcept { return handle_; }

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

  template <class F> decltype(auto) with_resource(F &&f) {
    return static_cast<F &&>(f)(resource_);
  }

  template <class F> decltype(auto) with_resource(F &&f) const {
    return static_cast<F &&>(f)(resource_);
  }

  explicit operator bool() const noexcept { return manager_ != nullptr; }

  void release() {
    if (manager_) {
      manager_->release(*this);
      manager_ = nullptr;
    }
  }

private:
  SharedLease(ManagerT *manager, HandleT handle, ResourceT resource)
      : manager_(manager), handle_(std::move(handle)),
        resource_(std::move(resource)) {}

  void copyFrom(const SharedLease &other) {
    if (other.manager_) {
      *this = other.manager_->acquire(other.handle_);
    }
  }

  void invalidate() noexcept {
    manager_ = nullptr;
    handle_ = HandleT{};
    resource_ = ResourceT{};
  }

  ResourceT &getForManager() noexcept { return resource_; }
  const ResourceT &getForManager() const noexcept { return resource_; }

  ManagerT *manager_{nullptr};
  HandleT handle_{};
  ResourceT resource_{};
};

} // namespace orteaf::internal::runtime::base

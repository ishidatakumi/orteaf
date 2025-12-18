#pragma once

#include <memory>
#include <type_traits>
#include <utility>

#include <orteaf/internal/runtime/base/lease/category.h>

namespace orteaf::internal::runtime::base {

/// @brief Raw lease - no lifecycle management, just a handle wrapper
/// @details Used for resources that don't need reference counting or ownership
/// tracking. The manager is responsible for all lifecycle management.
template <class HandleT, class ResourceT, class ManagerT> class RawLease {
  friend ManagerT;

public:
  using HandleType = HandleT;
  using ResourceType = ResourceT;
  using ManagerType = ManagerT;
  using CompatibleCategory = lease_category::Raw;

  RawLease() noexcept = default;

  // Copy increments weak reference count
  RawLease(const RawLease &other) noexcept
      : manager_(other.manager_), handle_(other.handle_),
        resource_(other.resource_) {
    if (manager_ && handle_.isValid()) {
      manager_->acquireExisting(handle_);
    }
  }

  RawLease &operator=(const RawLease &other) noexcept {
    if (this != &other) {
      release(); // Release current before copying
      manager_ = other.manager_;
      handle_ = other.handle_;
      resource_ = other.resource_;
      if (manager_ && handle_.isValid()) {
        manager_->acquireExisting(handle_);
      }
    }
    return *this;
  }

  RawLease(RawLease &&other) noexcept
      : manager_(std::exchange(other.manager_, nullptr)),
        handle_(std::move(other.handle_)),
        resource_(std::move(other.resource_)) {}

  RawLease &operator=(RawLease &&other) noexcept {
    if (this != &other) {
      release();
      manager_ = std::exchange(other.manager_, nullptr);
      handle_ = std::move(other.handle_);
      resource_ = std::move(other.resource_);
    }
    return *this;
  }

  ~RawLease() noexcept { release(); }

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

  explicit operator bool() const noexcept { return manager_ != nullptr; }

  void release() noexcept {
    if (manager_) {
      manager_->release(*this);
      manager_ = nullptr;
    }
  }

private:
  RawLease(ManagerT *mgr, HandleT handle, ResourceT resource) noexcept
      : manager_(mgr), handle_(std::move(handle)),
        resource_(std::move(resource)) {}

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

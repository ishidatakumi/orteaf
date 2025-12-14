#pragma once

#include <memory>
#include <type_traits>
#include <utility>

#include <orteaf/internal/runtime/base/lease/category.h>

namespace orteaf::internal::runtime::base {

/// @brief Unique lease - single ownership with exclusive access
/// @details Only one lease can hold a resource at a time. Similar to
/// unique_ptr. Destruction releases the resource back to the manager.
template <class HandleT, class ResourceT, class ManagerT> class UniqueLease {
  friend ManagerT;

public:
  using HandleType = HandleT;
  using ResourceType = ResourceT;
  using ManagerType = ManagerT;
  using CompatibleCategory = lease_category::Unique;

  UniqueLease() noexcept = default;
  UniqueLease(const UniqueLease &) = delete;
  UniqueLease &operator=(const UniqueLease &) = delete;

  UniqueLease(UniqueLease &&other) noexcept
      : manager_(std::exchange(other.manager_, nullptr)),
        handle_(std::move(other.handle_)),
        resource_(std::move(other.resource_)) {}

  UniqueLease &operator=(UniqueLease &&other) noexcept {
    if (this != &other) {
      release();
      manager_ = std::exchange(other.manager_, nullptr);
      handle_ = std::move(other.handle_);
      resource_ = std::move(other.resource_);
    }
    return *this;
  }

  ~UniqueLease() noexcept { release(); }

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

  void release() noexcept {
    if (manager_) {
      manager_->release(*this);
      manager_ = nullptr;
    }
  }

#if ORTEAF_ENABLE_TEST
  static UniqueLease makeForTest(HandleT handle, ResourceT resource) noexcept {
    return UniqueLease{nullptr, std::move(handle), std::move(resource)};
  }
#endif

private:
  UniqueLease(ManagerT *mgr, HandleT handle, ResourceT resource) noexcept
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

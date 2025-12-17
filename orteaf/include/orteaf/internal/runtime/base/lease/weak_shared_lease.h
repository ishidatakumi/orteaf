#pragma once

#include <memory>
#include <type_traits>
#include <utility>

#include <orteaf/internal/runtime/base/lease/category.h>

namespace orteaf::internal::runtime::base {

/// @brief Weak shared lease - weak reference to a shared resource
/// @details Similar to std::weak_ptr. Can observe a resource without
/// preventing its destruction. Can be promoted to a strong SharedLease
/// if the resource is still alive (has strong references).
template <class HandleT, class ResourceT, class ManagerT>
class WeakSharedLease {
  friend ManagerT;

public:
  using HandleType = HandleT;
  using ResourceType = ResourceT;
  using ManagerType = ManagerT;
  using CompatibleCategory =
      ::orteaf::internal::base::lease_category::WeakShared;

  WeakSharedLease() noexcept = default;

  WeakSharedLease(const WeakSharedLease &other) noexcept { copyFrom(other); }

  WeakSharedLease &operator=(const WeakSharedLease &other) noexcept {
    if (this != &other) {
      release();
      copyFrom(other);
    }
    return *this;
  }

  WeakSharedLease(WeakSharedLease &&other) noexcept
      : manager_(std::exchange(other.manager_, nullptr)),
        handle_(std::move(other.handle_)) {}

  WeakSharedLease &operator=(WeakSharedLease &&other) noexcept {
    if (this != &other) {
      release();
      manager_ = std::exchange(other.manager_, nullptr);
      handle_ = std::move(other.handle_);
    }
    return *this;
  }

  ~WeakSharedLease() noexcept { release(); }

  const HandleT &handle() const noexcept { return handle_; }

  /// @brief Check if the referenced resource is still alive
  bool expired() const noexcept {
    return manager_ == nullptr || !manager_->isAlive(handle_);
  }

  /// @brief Get the current use count (strong references)
  std::size_t use_count() const noexcept {
    if (manager_) {
      return manager_->useCount(handle_);
    }
    return 0;
  }

  /// @brief Try to promote to a strong shared lease
  /// @return A valid SharedLease if successful, invalid otherwise
  auto lock() const {
    if (manager_) {
      return manager_->tryPromote(handle_);
    }
    return typename ManagerT::SharedLease{};
  }

  explicit operator bool() const noexcept { return manager_ != nullptr; }

  void release() noexcept {
    if (manager_) {
      manager_->releaseWeak(*this);
      manager_ = nullptr;
    }
  }

private:
  WeakSharedLease(ManagerT *manager, HandleT handle) noexcept
      : manager_(manager), handle_(std::move(handle)) {}

  void copyFrom(const WeakSharedLease &other) noexcept {
    if (other.manager_) {
      manager_ = other.manager_;
      handle_ = other.handle_;
      manager_->acquireWeak(handle_);
    }
  }

  void invalidate() noexcept {
    manager_ = nullptr;
    handle_ = HandleT{};
  }

  ManagerT *manager_{nullptr};
  HandleT handle_{};
};

} // namespace orteaf::internal::runtime::base

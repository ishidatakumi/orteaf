#pragma once

#include <memory>
#include <type_traits>
#include <utility>

#include <orteaf/internal/runtime/base/lease/category.h>

namespace orteaf::internal::runtime::base {

/// @brief Weak unique lease - weak reference to a uniquely owned resource
/// @details Can observe a resource without preventing its destruction.
/// Can be promoted to a strong UniqueLease if the resource is still alive.
template <class HandleT, class ResourceT, class ManagerT>
class WeakUniqueLease {
  friend ManagerT;

public:
  using HandleType = HandleT;
  using ResourceType = ResourceT;
  using ManagerType = ManagerT;
  using CompatibleCategory =
      ::orteaf::internal::base::lease_category::WeakUnique;

  WeakUniqueLease() noexcept = default;

  WeakUniqueLease(const WeakUniqueLease &other) noexcept { copyFrom(other); }

  WeakUniqueLease &operator=(const WeakUniqueLease &other) noexcept {
    if (this != &other) {
      release();
      copyFrom(other);
    }
    return *this;
  }

  WeakUniqueLease(WeakUniqueLease &&other) noexcept
      : manager_(std::exchange(other.manager_, nullptr)),
        handle_(std::move(other.handle_)) {}

  WeakUniqueLease &operator=(WeakUniqueLease &&other) noexcept {
    if (this != &other) {
      release();
      manager_ = std::exchange(other.manager_, nullptr);
      handle_ = std::move(other.handle_);
    }
    return *this;
  }

  ~WeakUniqueLease() noexcept { release(); }

  const HandleT &handle() const noexcept { return handle_; }

  /// @brief Check if the referenced resource is still alive
  bool expired() const noexcept {
    return manager_ == nullptr || !manager_->isAlive(handle_);
  }

  /// @brief Try to promote to a strong unique lease
  /// @return A valid UniqueLease if successful, invalid otherwise
  auto lock() const {
    if (manager_) {
      return manager_->tryPromote(handle_);
    }
    return typename ManagerT::UniqueLease{};
  }

  explicit operator bool() const noexcept { return manager_ != nullptr; }

  void release() noexcept {
    if (manager_) {
      manager_->releaseWeak(*this);
      manager_ = nullptr;
    }
  }

private:
  WeakUniqueLease(ManagerT *manager, HandleT handle) noexcept
      : manager_(manager), handle_(std::move(handle)) {}

  void copyFrom(const WeakUniqueLease &other) noexcept {
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

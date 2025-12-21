#pragma once

#include <type_traits>
#include <utility>

#include <orteaf/internal/runtime/base/lease/category.h>
#include <orteaf/internal/runtime/base/lease/strong_lease.h>

namespace orteaf::internal::runtime::base {

/// @brief Weak lease - weak ownership via control block
/// @details Lease holds control block pointer, handle, and control block pool.
/// Weak locks can promote to StrongLease without manager involvement.
template <class HandleT, class ControlBlockT, class PoolT, class ManagerT>
class WeakLease {
  friend ManagerT;

public:
  using HandleType = HandleT;
  using ControlBlockType = ControlBlockT;
  using PoolType = PoolT;
  using ManagerType = ManagerT;
  using CompatibleCategory = lease_category::WeakShared;
  using StrongLeaseType =
      StrongLease<HandleT, ControlBlockT, PoolT, ManagerT>;

  WeakLease() noexcept = default;

  WeakLease(const WeakLease &other) noexcept { copyFrom(other); }

  WeakLease &operator=(const WeakLease &other) noexcept {
    if (this != &other) {
      release();
      copyFrom(other);
    }
    return *this;
  }

  WeakLease(WeakLease &&other) noexcept
      : control_block_(std::exchange(other.control_block_, nullptr)),
        pool_(std::exchange(other.pool_, nullptr)),
        handle_(std::move(other.handle_)) {}

  WeakLease &operator=(WeakLease &&other) noexcept {
    if (this != &other) {
      release();
      control_block_ = std::exchange(other.control_block_, nullptr);
      pool_ = std::exchange(other.pool_, nullptr);
      handle_ = std::move(other.handle_);
    }
    return *this;
  }

  ~WeakLease() noexcept { release(); }

  const HandleT &handle() const noexcept { return handle_; }

  ControlBlockT *controlBlock() noexcept { return control_block_; }
  const ControlBlockT *controlBlock() const noexcept { return control_block_; }

  /// @brief Try to promote to a strong lease
  /// @return A valid StrongLease if successful, invalid otherwise
  StrongLeaseType lock() noexcept {
    if (control_block_ == nullptr) {
      return StrongLeaseType{};
    }
    if constexpr (requires { control_block_->tryPromote(); }) {
      if (control_block_->tryPromote()) {
        return StrongLeaseType::adopt(control_block_, pool_, handle_);
      }
    }
    return StrongLeaseType{};
  }

  explicit operator bool() const noexcept { return control_block_ != nullptr; }

  void release() noexcept {
    if (!control_block_) {
      return;
    }
    const bool released = control_block_->releaseWeak();
    if (released && pool_ != nullptr && control_block_->canShutdown()) {
      pool_->release(handle_);
    }
    invalidate();
  }

private:
  WeakLease(ControlBlockT *control_block, PoolT *pool, HandleT handle) noexcept
      : control_block_(control_block), pool_(pool), handle_(std::move(handle)) {
    if (control_block_) {
      control_block_->acquireWeak();
    }
  }

  void copyFrom(const WeakLease &other) noexcept {
    if (other.control_block_ != nullptr) {
      control_block_ = other.control_block_;
      pool_ = other.pool_;
      handle_ = other.handle_;
      control_block_->acquireWeak();
    }
  }

  static HandleT invalidHandle() noexcept {
    if constexpr (requires { HandleT::invalid(); }) {
      return HandleT::invalid();
    } else {
      return HandleT{};
    }
  }

  void invalidate() noexcept {
    control_block_ = nullptr;
    pool_ = nullptr;
    handle_ = invalidHandle();
  }

  ControlBlockT *control_block_{nullptr};
  PoolT *pool_{nullptr};
  HandleT handle_{};
};

} // namespace orteaf::internal::runtime::base

#pragma once

#include <type_traits>
#include <utility>

#include <orteaf/internal/runtime/base/lease/category.h>

namespace orteaf::internal::runtime::base {

template <class HandleT, class ControlBlockT, class PoolT, class ManagerT>
class StrongLease;

template <class HandleT, class ControlBlockT, class PoolT, class ManagerT>
class WeakLease;

/// @brief Strong lease - shared ownership via control block
/// @details Lease holds control block pointer, handle, and control block pool.
/// Counts are managed directly on the control block.
template <class HandleT, class ControlBlockT, class PoolT, class ManagerT>
class StrongLease {
  friend ManagerT;
  friend class WeakLease<HandleT, ControlBlockT, PoolT, ManagerT>;

public:
  using HandleType = HandleT;
  using ControlBlockType = ControlBlockT;
  using PoolType = PoolT;
  using ManagerType = ManagerT;
  using CompatibleCategory = lease_category::Shared;

  StrongLease() = default;

  StrongLease(const StrongLease &other) { copyFrom(other); }

  StrongLease &operator=(const StrongLease &other) {
    if (this != &other) {
      release();
      copyFrom(other);
    }
    return *this;
  }

  StrongLease(StrongLease &&other) noexcept
      : control_block_(std::exchange(other.control_block_, nullptr)),
        pool_(std::exchange(other.pool_, nullptr)),
        handle_(std::move(other.handle_)) {}

  StrongLease &operator=(StrongLease &&other) noexcept {
    if (this != &other) {
      release();
      control_block_ = std::exchange(other.control_block_, nullptr);
      pool_ = std::exchange(other.pool_, nullptr);
      handle_ = std::move(other.handle_);
    }
    return *this;
  }

  ~StrongLease() { release(); }

  const HandleT &handle() const noexcept { return handle_; }

  ControlBlockT *controlBlock() noexcept { return control_block_; }
  const ControlBlockT *controlBlock() const noexcept { return control_block_; }

  explicit operator bool() const noexcept { return control_block_ != nullptr; }

  void release() noexcept {
    if (!control_block_) {
      return;
    }
    const bool released = control_block_->release();
    if (released && pool_ != nullptr && control_block_->canShutdown()) {
      pool_->release(handle_);
    }
    invalidate();
  }

private:
  struct AdoptTag {
    explicit AdoptTag() = default;
  };

  StrongLease(ControlBlockT *control_block, PoolT *pool, HandleT handle)
      : control_block_(control_block), pool_(pool), handle_(std::move(handle)) {
    if (control_block_) {
      control_block_->acquire();
    }
  }

  StrongLease(ControlBlockT *control_block, PoolT *pool, HandleT handle,
              AdoptTag)
      : control_block_(control_block), pool_(pool), handle_(std::move(handle)) {}

  void copyFrom(const StrongLease &other) {
    if (other.control_block_ != nullptr) {
      control_block_ = other.control_block_;
      pool_ = other.pool_;
      handle_ = other.handle_;
      control_block_->acquire();
    }
  }

  static StrongLease adopt(ControlBlockT *control_block, PoolT *pool,
                           HandleT handle) {
    return StrongLease(control_block, pool, std::move(handle), AdoptTag{});
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

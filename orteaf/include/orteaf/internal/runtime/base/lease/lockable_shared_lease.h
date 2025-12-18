#pragma once

#include <memory>
#include <optional>
#include <type_traits>

#include <orteaf/internal/base/handle.h>
#include <orteaf/internal/diagnostics/error/error.h>
#include <orteaf/internal/runtime/base/lease/category.h>
#include <orteaf/internal/runtime/base/lease/concepts.h>

namespace orteaf::internal::runtime::base {

/// @brief Lease for lockable shared resources
/// @details Holds a reference to a shared resource but does NOT provide direct
/// access. Access must be obtained via lock() (exclusive) or accessConcurrent()
/// (unsafe).
template <typename HandleT, typename PayloadT, typename ManagerT>
class LockableSharedLease {
public:
  using Handle = HandleT;
  using Payload = PayloadT;
  using Manager = ManagerT;
  // Compatible with Shared category (uses ref counting)
  using CompatibleCategory = lease_category::Shared;

  // =========================================================================
  // Scoped Lock (Accessor)
  // =========================================================================

  /// @brief RAII wrapper for exclusive access
  class ScopedLock {
  public:
    ScopedLock(LockableSharedLease &lease) : lease_(&lease) {}

    ScopedLock(ScopedLock &&other) noexcept : lease_(other.lease_) {
      other.lease_ = nullptr;
    }

    ScopedLock &operator=(ScopedLock &&other) noexcept {
      if (this != &other) {
        if (lease_) {
          unlock();
        }
        lease_ = other.lease_;
        other.lease_ = nullptr;
      }
      return *this;
    }

    ~ScopedLock() {
      if (lease_) {
        unlock();
      }
    }

    // Accessors
    Payload *operator->() const noexcept {
      return &lease_->manager_->getPayload(*lease_);
    }
    Payload &operator*() const noexcept {
      return lease_->manager_->getPayload(*lease_);
    }

    // Bool check
    explicit operator bool() const noexcept { return lease_ != nullptr; }

  private:
    void unlock() {
      if (lease_ && lease_->isValid()) {
        lease_->manager_->unlock(*lease_);
      }
      lease_ = nullptr;
    }

    LockableSharedLease *lease_{nullptr};
  };

  // =========================================================================
  // Constructors
  // =========================================================================

  LockableSharedLease() = default;

  LockableSharedLease(HandleT handle, ManagerT *manager)
      : handle_(handle), manager_(manager) {}

  LockableSharedLease(const LockableSharedLease &other)
      : handle_(other.handle_), manager_(other.manager_) {
    if (isValid()) {
      manager_->acquireExisting(handle_);
    }
  }

  LockableSharedLease(LockableSharedLease &&other) noexcept
      : handle_(other.handle_), manager_(other.manager_) {
    other.handle_ = HandleT::invalid();
    other.manager_ = nullptr;
  }

  LockableSharedLease &operator=(const LockableSharedLease &other) {
    if (this != &other) {
      release(); // Release current
      handle_ = other.handle_;
      manager_ = other.manager_;
      if (isValid()) {
        manager_->acquireExisting(handle_);
      }
    }
    return *this;
  }

  LockableSharedLease &operator=(LockableSharedLease &&other) noexcept {
    if (this != &other) {
      release(); // Release current
      handle_ = other.handle_;
      manager_ = other.manager_;
      other.handle_ = HandleT::invalid();
      other.manager_ = nullptr;
    }
    return *this;
  }

  ~LockableSharedLease() { release(); }

  // =========================================================================
  // Access API
  // =========================================================================

  /// @brief Try to acquire exclusive lock
  /// @return ScopedLock if successful, empty/invalid ScopedLock optional?
  /// @note For simplicity, returning ScopedLock that evaluates to true/false.
  ///       Or should we return std::optional<ScopedLock>?
  ///       Let's stick to the plan: tryLock returns ScopedLock (lockable
  ///       object). Actually, manager->tryLock returns a lock object or fails.
  ///       Wait, if we use RAII, the constructor has to succeed or we shouldn't
  ///       construct. Let's use std::optional<ScopedLock> for tryLock.
  std::optional<ScopedLock> tryLock() {
    if (!isValid())
      return std::nullopt;
    if (manager_->tryLock(*this)) {
      return ScopedLock(*this);
    }
    return std::nullopt;
  }

  // Future: blocking lock() could be added here if supported by manager

  /// @brief Access payload explicitly for concurrent use (Unsafe/No Lock)
  /// @return Reference to payload
  Payload &accessConcurrent() const {
    if (!isValid()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "Attempt to access invalid lease");
    }
    return manager_->getPayload(*this);
  }

  // =========================================================================
  // Lifecycle API
  // =========================================================================

  void release() {
    if (isValid() && manager_) {
      manager_->release(*this);
      handle_ = HandleT::invalid();
      manager_ = nullptr;
    }
  }

  void invalidate() noexcept {
    handle_ = HandleT::invalid();
    manager_ = nullptr;
  }

  bool isValid() const noexcept {
    return manager_ != nullptr && handle_.isValid();
  }

  explicit operator bool() const noexcept { return isValid(); }

  HandleT handle() const noexcept { return handle_; }

private:
  HandleT handle_{HandleT::invalid()};
  ManagerT *manager_{nullptr};

  // Friend for ScopedLock
  friend class ScopedLock;
};

} // namespace orteaf::internal::runtime::base

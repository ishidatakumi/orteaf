#pragma once

#include <type_traits>
#include <utility>

#include "orteaf/internal/execution/base/lease/category.h"
#include "orteaf/internal/execution/base/lease/strong_lease.h"

namespace orteaf::internal::execution::base {

/**
 * @brief Weak lease providing non-owning reference to a resource via control
 * block.
 *
 * WeakLease is a lightweight handle that represents weak (non-owning) access to
 * a resource managed by a control block. Weak references do not prevent the
 * resource from being destroyed, but can be promoted to StrongLease via
 * `lock()`.
 *
 * Key characteristics:
 * - **Non-owning**: Does not prevent payload destruction (controlled by strong
 * count).
 * - **Promotable**: Can attempt to upgrade to StrongLease via `lock()`.
 * - **Direct access**: Accesses control block directly without manager
 * involvement.
 * - **Safe expiration**: `lock()` fails gracefully if resource was destroyed.
 *
 * Use cases:
 * - Caching references that may expire.
 * - Breaking reference cycles.
 * - Observing resources without extending their lifetime.
 *
 * Lifetime management:
 * - Construction increments weak count via `ControlBlock::acquireWeak()`.
 * - Copy increments weak count.
 * - Move transfers ownership without count change.
 * - Destruction/release decrements weak count via
 * `ControlBlock::releaseWeak()`.
 * - When `releaseWeak()` returns true and `canShutdown()` is true, the control
 *   block is returned to the pool.
 *
 * @tparam HandleT Handle type for control block identification (includes
 * generation).
 * @tparam ControlBlockT Control block type managing reference counts and
 * payload.
 * @tparam PoolT Pool type for control block storage and recycling.
 * @tparam ManagerT Manager type that creates leases (friend for private
 * access).
 *
 * @see StrongLease For owning references that prevent resource destruction.
 * @see lease-design.md For design rationale and usage patterns.
 */
template <class HandleT, class ControlBlockT, class PoolT, class ManagerT>
class WeakLease {
  friend ManagerT;

public:
  /// @brief Handle type used for control block identification.
  using HandleType = HandleT;
  /// @brief Control block type managing counts and payload.
  using ControlBlockType = ControlBlockT;
  /// @brief Pool type for control block storage.
  using PoolType = PoolT;
  /// @brief Manager type that creates leases.
  using ManagerType = ManagerT;
  /// @brief Category tag indicating this lease is compatible with shared leases.
  using CompatibleCategory = lease_category::Shared;
  /// @brief The corresponding strong lease type for promotion.
  using StrongLeaseType = StrongLease<HandleT, ControlBlockT, PoolT, ManagerT>;

  /**
   * @brief Default constructor creating an invalid (empty) lease.
   *
   * An invalid lease has no control block and evaluates to false.
   * `lock()` on an invalid lease returns an invalid StrongLease.
   */
  WeakLease() noexcept = default;

  /**
   * @brief Construct from StrongLease to create weak reference.
   * @param strong The strong lease to create weak reference from.
   *
   * If `strong` is valid, creates a weak reference to the same resource.
   * Requires that ControlBlockT has an `acquireWeak()` method.
   * Does not affect the strong reference count.
   */
  explicit WeakLease(const StrongLeaseType &strong) noexcept
    requires requires(ControlBlockT *cb) { cb->acquireWeak(); }
  {
    if (strong.control_block_ != nullptr) {
      control_block_ = strong.control_block_;
      pool_ = strong.pool_;
      handle_ = strong.handle_;
      control_block_->acquireWeak();
    }
  }

  /**
   * @brief Copy constructor incrementing weak reference count.
   * @param other The lease to copy from.
   *
   * If `other` is valid, this lease observes the same resource and
   * increments the control block's weak count via `acquireWeak()`.
   */
  WeakLease(const WeakLease &other) noexcept { copyFrom(other); }

  /**
   * @brief Copy assignment operator with proper cleanup.
   * @param other The lease to copy from.
   * @return Reference to this lease.
   *
   * Releases current weak reference (if any) before copying from `other`.
   * Self-assignment is handled safely.
   */
  WeakLease &operator=(const WeakLease &other) noexcept {
    if (this != &other) {
      release();
      copyFrom(other);
    }
    return *this;
  }

  /**
   * @brief Move constructor transferring weak reference without count change.
   * @param other The lease to move from (will be invalidated).
   *
   * After move, `other` becomes invalid and this lease holds the weak
   * reference. No reference count changes occur.
   */
  WeakLease(WeakLease &&other) noexcept
      : control_block_(std::exchange(other.control_block_, nullptr)),
        pool_(std::exchange(other.pool_, nullptr)),
        handle_(std::move(other.handle_)) {}

  /**
   * @brief Move assignment operator with proper cleanup.
   * @param other The lease to move from (will be invalidated).
   * @return Reference to this lease.
   *
   * Releases current weak reference before taking reference from `other`.
   */
  WeakLease &operator=(WeakLease &&other) noexcept {
    if (this != &other) {
      release();
      control_block_ = std::exchange(other.control_block_, nullptr);
      pool_ = std::exchange(other.pool_, nullptr);
      handle_ = std::move(other.handle_);
    }
    return *this;
  }

  /**
   * @brief Destructor releasing weak reference.
   *
   * Calls `release()` to decrement weak count and potentially
   * return control block to pool.
   */
  ~WeakLease() noexcept { release(); }

  /**
   * @brief Get the control block handle.
   * @return Const reference to the handle.
   *
   * The handle contains index and generation (if applicable) for
   * stale reference detection.
   */
  const HandleT &handle() const noexcept { return handle_; }

  /**
   * @brief Get mutable pointer to control block.
   * @return Pointer to control block, or nullptr if invalid.
   */
  ControlBlockT *controlBlock() noexcept { return control_block_; }

  /**
   * @brief Get const pointer to control block.
   * @return Const pointer to control block, or nullptr if invalid.
   */
  const ControlBlockT *controlBlock() const noexcept { return control_block_; }

  /**
   * @brief Get the payload handle from control block.
   * @return Payload handle if valid, invalid handle otherwise.
   *
   * Only available if ControlBlockT has a `payloadHandle()` method.
   * @note The payload may have been destroyed even if handle is valid.
   */
  auto payloadHandle() const noexcept
      -> decltype(std::declval<const ControlBlockT *>()->payloadHandle())
    requires requires(const ControlBlockT *cb) { cb->payloadHandle(); }
  {
    using PayloadHandleT =
        decltype(std::declval<const ControlBlockT *>()->payloadHandle());
    if (!control_block_) {
      if constexpr (requires { PayloadHandleT::invalid(); }) {
        return PayloadHandleT::invalid();
      } else {
        return PayloadHandleT{};
      }
    }
    return control_block_->payloadHandle();
  }

  /**
   * @brief Get mutable pointer to payload.
   * @return Pointer to payload, or nullptr if invalid.
   *
   * Only available if ControlBlockT has a `payloadPtr()` method.
   * @warning The payload may be destroyed even if pointer is non-null.
   * Prefer using `lock()` for safe access.
   */
  auto payloadPtr() noexcept
      -> decltype(std::declval<ControlBlockT *>()->payloadPtr())
    requires requires(ControlBlockT *cb) { cb->payloadPtr(); }
  {
    return control_block_ ? control_block_->payloadPtr() : nullptr;
  }

  /**
   * @brief Get const pointer to payload.
   * @return Const pointer to payload, or nullptr if invalid.
   *
   * Only available if ControlBlockT has a `payloadPtr()` method.
   * @warning The payload may be destroyed even if pointer is non-null.
   */
  auto payloadPtr() const noexcept
      -> decltype(std::declval<const ControlBlockT *>()->payloadPtr())
    requires requires(const ControlBlockT *cb) { cb->payloadPtr(); }
  {
    return control_block_ ? control_block_->payloadPtr() : nullptr;
  }

  /**
   * @brief Attempt to promote weak reference to strong ownership.
   * @return Valid StrongLease if promotion succeeded, invalid otherwise.
   *
   * This method attempts to atomically increment the strong count via
   * `ControlBlock::tryPromote()`. If the resource has already been
   * destroyed (strong count reached 0), promotion fails.
   *
   * Success conditions:
   * - This lease is valid (has control block).
   * - ControlBlock supports `tryPromote()`.
   * - `tryPromote()` returns true (resource still alive).
   *
   * On success, returns a StrongLease that shares ownership. The returned
   * lease adopts the promoted reference (no additional acquire).
   *
   * On failure, returns an invalid StrongLease. This weak lease remains valid.
   */
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

  /**
   * @brief Get the weak reference count from control block.
   * @return The weak reference count, or 0 if invalid.
   *
   * Only available if ControlBlockT has a `weakCount()` method.
   * Useful for testing and debugging reference counting behavior.
   */
  auto weakCount() const noexcept
      -> decltype(std::declval<const ControlBlockT *>()->weakCount())
    requires requires(const ControlBlockT *cb) { cb->weakCount(); }
  {
    return control_block_ ? control_block_->weakCount() : 0;
  }

  /**
   * @brief Check if this lease is valid (holds a weak reference).
   * @return true if valid, false if invalid/empty.
   *
   * A valid weak lease has a non-null control block pointer.
   * @note Being valid does NOT mean the resource is still alive.
   * Use `lock()` to safely access the resource.
   */
  explicit operator bool() const noexcept { return control_block_ != nullptr; }

  /**
   * @brief Release weak reference and potentially return to pool.
   *
   * This method:
   * 1. If already invalid, does nothing.
   * 2. Calls `ControlBlock::releaseWeak()` to decrement weak count.
   * 3. If releaseWeak indicates last reference and `canShutdown()` is true,
   *    returns control block to pool via `Pool::release()`.
   * 4. Invalidates this lease.
   *
   * After calling, this lease becomes invalid. Safe to call multiple times.
   * Does not throw exceptions (ignores double-release).
   */
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
  /**
   * @brief Private constructor for manager use - acquires weak reference.
   * @param control_block Pointer to control block.
   * @param pool Pointer to control block pool.
   * @param handle Control block handle.
   *
   * Calls `acquireWeak()` on control block to increment weak count.
   */
  WeakLease(ControlBlockT *control_block, PoolT *pool, HandleT handle) noexcept
      : control_block_(control_block), pool_(pool), handle_(std::move(handle)) {
    if (control_block_) {
      control_block_->acquireWeak();
    }
  }

  /**
   * @brief Copy state from another lease and acquire weak reference.
   * @param other Source lease to copy from.
   */
  void copyFrom(const WeakLease &other) noexcept {
    if (other.control_block_ != nullptr) {
      control_block_ = other.control_block_;
      pool_ = other.pool_;
      handle_ = other.handle_;
      control_block_->acquireWeak();
    }
  }

  /// @brief Get invalid handle value.
  static HandleT invalidHandle() noexcept {
    if constexpr (requires { HandleT::invalid(); }) {
      return HandleT::invalid();
    } else {
      return HandleT{};
    }
  }

  /// @brief Reset all members to invalid state.
  void invalidate() noexcept {
    control_block_ = nullptr;
    pool_ = nullptr;
    handle_ = invalidHandle();
  }

  ControlBlockT *control_block_{nullptr}; ///< Pointer to control block.
  PoolT *pool_{nullptr};                  ///< Pointer to control block pool.
  HandleT handle_{};                      ///< Control block handle.
};

} // namespace orteaf::internal::execution::base

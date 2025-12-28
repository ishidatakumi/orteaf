#pragma once

#include <type_traits>
#include <utility>

#include "orteaf/internal/base/lease/category.h"

namespace orteaf::internal::base {

template <class HandleT, class ControlBlockT, class PoolT, class ManagerT>
class StrongLease;

template <class HandleT, class ControlBlockT, class PoolT, class ManagerT>
class WeakLease;

/**
 * @brief Strong lease providing shared ownership of a resource via control
 * block.
 *
 * StrongLease is a lightweight handle that represents strong (owning) access to
 * a resource managed by a control block. Each StrongLease increments the strong
 * reference count on construction/copy and decrements it on
 * destruction/release.
 *
 * Key characteristics:
 * - **Lightweight**: Designed to be created on each acquire (not pooled).
 * - **Direct access**: Accesses control block directly without manager
 * involvement.
 * - **Reference counted**: Strong count prevents payload destruction while
 * active.
 * - **Pool return**: Returns control block to pool when all references are
 * released.
 *
 * Lifetime management:
 * - Construction increments strong count via `ControlBlock::acquire()`.
 * - Copy increments strong count.
 * - Move transfers ownership without count change.
 * - Destruction/release decrements strong count via `ControlBlock::release()`.
 * - When `release()` returns true and `canShutdown()` is true, the control
 * block is returned to the pool.
 *
 * @tparam HandleT Handle type for control block identification (includes
 * generation).
 * @tparam ControlBlockT Control block type managing reference counts and
 * payload.
 * @tparam PoolT Pool type for control block storage and recycling.
 * @tparam ManagerT Manager type that creates leases (friend for private
 * access).
 *
 * @see WeakLease For non-owning references that can be promoted to StrongLease.
 * @see lease-design.md For design rationale and usage patterns.
 */
template <class HandleT, class ControlBlockT, class PoolT, class ManagerT>
class StrongLease {
  friend ManagerT;
  friend class WeakLease<HandleT, ControlBlockT, PoolT, ManagerT>;

public:
  /// @brief Handle type used for control block identification.
  using HandleType = HandleT;
  /// @brief Control block type managing counts and payload.
  using ControlBlockType = ControlBlockT;
  /// @brief Pool type for control block storage.
  using PoolType = PoolT;
  /// @brief Manager type that creates leases.
  using ManagerType = ManagerT;
  /// @brief Category tag indicating this is a strong lease.
  using CompatibleCategory = lease_category::Strong;

  /**
   * @brief Default constructor creating an invalid (empty) lease.
   *
   * An invalid lease has no control block and evaluates to false.
   * Safe to destroy or assign to.
   */
  StrongLease() = default;

  /**
   * @brief Copy constructor incrementing strong reference count.
   * @param other The lease to copy from.
   *
   * If `other` is valid, this lease shares ownership and increments
   * the control block's strong count via `acquire()`.
   */
  StrongLease(const StrongLease &other) { copyFrom(other); }

  /**
   * @brief Copy assignment operator with proper cleanup.
   * @param other The lease to copy from.
   * @return Reference to this lease.
   *
   * Releases current ownership (if any) before copying from `other`.
   * Self-assignment is handled safely.
   */
  StrongLease &operator=(const StrongLease &other) {
    if (this != &other) {
      release();
      copyFrom(other);
    }
    return *this;
  }

  /**
   * @brief Move constructor transferring ownership without count change.
   * @param other The lease to move from (will be invalidated).
   *
   * After move, `other` becomes invalid and this lease owns the reference.
   * No reference count changes occur.
   */
  StrongLease(StrongLease &&other) noexcept
      : control_block_(std::exchange(other.control_block_, nullptr)),
        pool_(std::exchange(other.pool_, nullptr)),
        handle_(std::move(other.handle_)) {}

  /**
   * @brief Move assignment operator with proper cleanup.
   * @param other The lease to move from (will be invalidated).
   * @return Reference to this lease.
   *
   * Releases current ownership before taking ownership from `other`.
   */
  StrongLease &operator=(StrongLease &&other) noexcept {
    if (this != &other) {
      release();
      control_block_ = std::exchange(other.control_block_, nullptr);
      pool_ = std::exchange(other.pool_, nullptr);
      handle_ = std::move(other.handle_);
    }
    return *this;
  }

  /**
   * @brief Destructor releasing strong reference.
   *
   * Calls `release()` to decrement strong count and potentially
   * return control block to pool.
   */
  ~StrongLease() { release(); }

  /**
   * @brief Get the control block handle.
   * @return Const reference to the handle.
   *
   * The handle contains index and generation (if applicable) for
   * stale reference detection.
   */
  const HandleT &handle() const noexcept { return handle_; }

  /**
   * @brief Get the payload handle from control block.
   * @return Payload handle if valid, invalid handle otherwise.
   *
   * Only available if ControlBlockT has a `payloadHandle()` method.
   * Returns the handle to the actual resource managed by this lease.
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
   * Provides direct access to the managed resource.
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
   */
  auto payloadPtr() const noexcept
      -> decltype(std::declval<const ControlBlockT *>()->payloadPtr())
    requires requires(const ControlBlockT *cb) { cb->payloadPtr(); }
  {
    return control_block_ ? control_block_->payloadPtr() : nullptr;
  }

  /**
   * @brief Get the strong reference count from control block.
   * @return The strong reference count, or 0 if invalid.
   *
   * Only available if ControlBlockT has a `strongCount()` method.
   * Useful for testing and debugging reference counting behavior.
   */
  std::uint32_t strongCount() const noexcept
    requires requires(const ControlBlockT *cb) {
      { cb->strongCount() } -> std::convertible_to<std::uint32_t>;
    }
  {
    return control_block_ ? control_block_->strongCount() : 0;
  }

  /**
   * @brief Check if this lease is valid (owns a reference).
   * @return true if valid, false if invalid/empty.
   *
   * A valid lease has a non-null control block pointer.
   */
  explicit operator bool() const noexcept { return control_block_ != nullptr; }

  /**
   * @brief Release strong reference and potentially return to pool.
   *
   * This method:
   * 1. If already invalid, does nothing.
   * 2. Calls `ControlBlock::release()` to decrement strong count.
   * 3. If release indicates last reference and `canShutdown()` is true,
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
    const bool released = control_block_->releaseStrong();
    if (released && pool_ != nullptr && control_block_->canShutdown()) {
      pool_->release(handle_);
    }
    invalidate();
  }

private:
  /// @brief Tag type for adopt constructor (no acquire).
  struct AdoptTag {
    explicit AdoptTag() = default;
  };

  /**
   * @brief Private constructor for manager use - acquires reference.
   * @param control_block Pointer to control block.
   * @param pool Pointer to control block pool.
   * @param handle Control block handle.
   *
   * Calls `acquire()` on control block to increment strong count.
   */
  StrongLease(ControlBlockT *control_block, PoolT *pool, HandleT handle)
      : control_block_(control_block), pool_(pool), handle_(std::move(handle)) {
    if (control_block_) {
      control_block_->acquireStrong();
    }
  }

  /**
   * @brief Private constructor adopting existing reference (no acquire).
   * @param control_block Pointer to control block.
   * @param pool Pointer to control block pool.
   * @param handle Control block handle.
   * @param tag Adopt tag to differentiate from acquiring constructor.
   *
   * Used by WeakLease::lock() after successful tryPromote().
   */
  StrongLease(ControlBlockT *control_block, PoolT *pool, HandleT handle,
              AdoptTag)
      : control_block_(control_block), pool_(pool), handle_(std::move(handle)) {
  }

  /**
   * @brief Copy state from another lease and acquire reference.
   * @param other Source lease to copy from.
   */
  void copyFrom(const StrongLease &other) {
    if (other.control_block_ != nullptr) {
      control_block_ = other.control_block_;
      pool_ = other.pool_;
      handle_ = other.handle_;
      control_block_->acquireStrong();
    }
  }

  /**
   * @brief Create lease adopting existing reference.
   * @param control_block Pointer to control block.
   * @param pool Pointer to control block pool.
   * @param handle Control block handle.
   * @return StrongLease owning the reference.
   */
  static StrongLease adopt(ControlBlockT *control_block, PoolT *pool,
                           HandleT handle) {
    return StrongLease(control_block, pool, std::move(handle), AdoptTag{});
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

} // namespace orteaf::internal::base

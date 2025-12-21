#pragma once

#include <atomic>
#include <cstdint>

#include <orteaf/internal/runtime/base/lease/category.h>

namespace orteaf::internal::runtime::base {

/**
 * @brief Weak-only control block with handle/payload/pool binding.
 *
 * This control block tracks only weak references and does not own the payload.
 * It is intended for resources whose lifetime is managed externally (system-
 * or device-owned objects). The control block can still safely expose access
 * metadata (handle/pointer/pool) and validate that weak references remain.
 *
 * Payload binding is explicit and can only occur when no references exist.
 * The control block does not create/destroy payloads directly and does not
 * release payloads on weak count transition; it only tracks weak lifetime.
 *
 * Thread-safety: reference counts use atomics. Payload binding methods are not
 * synchronized and must be externally serialized.
 *
 * @tparam HandleT Handle type with Handle::invalid() and isValid().
 * @tparam PayloadT Payload type stored in the pool.
 * @tparam PoolT Pool type providing release(handle) -> bool (unused here).
 */
template <typename HandleT, typename PayloadT, typename PoolT>
class WeakControlBlock {
public:
  using Category = lease_category::Weak;
  using Handle = HandleT;
  using Payload = PayloadT;
  using Pool = PoolT;

  WeakControlBlock() = default;
  WeakControlBlock(const WeakControlBlock &) = delete;
  WeakControlBlock &operator=(const WeakControlBlock &) = delete;
  WeakControlBlock(WeakControlBlock &&) = default;
  WeakControlBlock &operator=(WeakControlBlock &&) = default;
  ~WeakControlBlock() = default;

  /**
   * @brief Returns true if payload binding is allowed.
   *
   * Binding is only allowed when there is no payload currently bound and the
   * weak reference count is zero.
   */
  bool canBindPayload() const noexcept {
    return payload_ptr_ == nullptr && weakCount() == 0;
  }

  /**
   * @brief Attempts to bind payload metadata to this control block.
   *
   * @param handle Handle corresponding to the payload slot.
   * @param payload Pointer to the payload storage.
   * @param pool Pointer to the owning pool (may be nullptr).
   * @return True if binding succeeded.
   */
  bool tryBindPayload(Handle handle, Payload *payload, Pool *pool) noexcept {
    if (!canBindPayload()) {
      return false;
    }
    bindPayload(handle, payload, pool);
    return true;
  }

  /**
   * @brief Returns true if a payload pointer is currently bound.
   */
  bool hasPayload() const noexcept { return payload_ptr_ != nullptr; }

  /**
   * @brief Returns the bound payload handle (may be invalid).
   */
  Handle payloadHandle() const noexcept { return payload_handle_; }
  /**
   * @brief Returns the bound payload pointer (may be null).
   */
  Payload *payloadPtr() noexcept { return payload_ptr_; }
  /**
   * @brief Const overload of payloadPtr().
   */
  const Payload *payloadPtr() const noexcept { return payload_ptr_; }
  /**
   * @brief Returns the bound pool pointer (may be null).
   */
  Pool *payloadPool() noexcept { return payload_pool_; }
  /**
   * @brief Const overload of payloadPool().
   */
  const Pool *payloadPool() const noexcept { return payload_pool_; }

  /**
   * @brief Increments the weak reference count.
   */
  void acquireWeak() noexcept {
    weak_count_.fetch_add(1, std::memory_order_relaxed);
  }

  /**
   * @brief Decrements the weak reference count.
   *
   * @return True if this call observed the transition from 1 to 0.
   */
  bool releaseWeak() noexcept {
    auto current = weak_count_.load(std::memory_order_acquire);
    while (current > 0) {
      if (weak_count_.compare_exchange_weak(current, current - 1,
                                            std::memory_order_acq_rel,
                                            std::memory_order_relaxed)) {
        return current == 1;
      }
    }
    return false;
  }

  /**
   * @brief Returns the current weak reference count.
   */
  std::uint32_t weakCount() const noexcept {
    return weak_count_.load(std::memory_order_acquire);
  }

  /**
   * @brief Returns true if the control block can be torn down.
   *
   * For WeakControlBlock, this is equivalent to weak count == 0.
   */
  bool canTeardown() const noexcept { return weakCount() == 0; }
  /**
   * @brief Returns true if the control block can be safely shutdown.
   *
   * For WeakControlBlock, this is equivalent to weak count == 0.
   */
  bool canShutdown() const noexcept { return weakCount() == 0; }

private:
  /**
   * @brief Binds payload metadata without validation.
   *
   * Callers should ensure canBindPayload() prior to calling.
   */
  void bindPayload(Handle handle, Payload *payload, Pool *pool) noexcept {
    payload_handle_ = handle;
    payload_ptr_ = payload;
    payload_pool_ = pool;
  }

  /**
   * @brief Clears payload metadata to invalid/null values.
   */
  void clearPayload() noexcept {
    payload_handle_ = Handle::invalid();
    payload_ptr_ = nullptr;
    payload_pool_ = nullptr;
  }

  std::atomic<std::uint32_t> weak_count_{0};
  Handle payload_handle_{Handle::invalid()};
  Payload *payload_ptr_{nullptr};
  Pool *payload_pool_{nullptr};
};

} // namespace orteaf::internal::runtime::base

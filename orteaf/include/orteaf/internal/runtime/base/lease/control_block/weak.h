#pragma once

#include <atomic>
#include <cstdint>

#include <orteaf/internal/runtime/base/lease/category.h>

namespace orteaf::internal::runtime::base {

/// @brief Weak control block - weak references only (no strong ownership)
/// @details Payload lifetime is managed externally; control block tracks only
/// weak references for safety and access.
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

  // Payload binding
  bool canBindPayload() const noexcept {
    return payload_ptr_ == nullptr && weakCount() == 0;
  }

  bool tryBindPayload(Handle handle, Payload *payload, Pool *pool) noexcept {
    if (!canBindPayload()) {
      return false;
    }
    bindPayload(handle, payload, pool);
    return true;
  }

  bool hasPayload() const noexcept { return payload_ptr_ != nullptr; }

  Handle payloadHandle() const noexcept { return payload_handle_; }
  Payload *payloadPtr() noexcept { return payload_ptr_; }
  const Payload *payloadPtr() const noexcept { return payload_ptr_; }
  Pool *payloadPool() noexcept { return payload_pool_; }
  const Pool *payloadPool() const noexcept { return payload_pool_; }

  // Weak reference API
  void acquireWeak() noexcept {
    weak_count_.fetch_add(1, std::memory_order_relaxed);
  }

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

  std::uint32_t weakCount() const noexcept {
    return weak_count_.load(std::memory_order_acquire);
  }

  bool canTeardown() const noexcept { return weakCount() == 0; }
  bool canShutdown() const noexcept { return weakCount() == 0; }

private:
  void bindPayload(Handle handle, Payload *payload, Pool *pool) noexcept {
    payload_handle_ = handle;
    payload_ptr_ = payload;
    payload_pool_ = pool;
  }

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

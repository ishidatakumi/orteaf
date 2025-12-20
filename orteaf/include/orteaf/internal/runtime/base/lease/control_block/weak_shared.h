#pragma once

#include <atomic>
#include <concepts>
#include <cstdint>
#include <type_traits>
#include <utility>

#include <orteaf/internal/runtime/base/lease/category.h>
#include <orteaf/internal/runtime/base/lease/slot.h>

namespace orteaf::internal::runtime::base {

// Primary template: handle/payload/pool based control block.
template <typename HandleT, typename PayloadT = void, typename PoolT = void>
class WeakSharedControlBlock {
public:
  using Category = lease_category::WeakShared;
  using Handle = HandleT;
  using Payload = PayloadT;
  using Pool = PoolT;

  WeakSharedControlBlock() = default;
  WeakSharedControlBlock(const WeakSharedControlBlock &) = delete;
  WeakSharedControlBlock &operator=(const WeakSharedControlBlock &) = delete;
  WeakSharedControlBlock(WeakSharedControlBlock &&) = default;
  WeakSharedControlBlock &operator=(WeakSharedControlBlock &&) = default;
  ~WeakSharedControlBlock() = default;

  // Payload binding
  bool canBindPayload() const noexcept {
    return payload_ptr_ == nullptr && count() == 0 && weakCount() == 0;
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

  // Strong reference API
  void acquire() noexcept {
    strong_count_.fetch_add(1, std::memory_order_relaxed);
  }

  bool release() noexcept {
    auto current = strong_count_.load(std::memory_order_acquire);
    while (current > 0) {
      if (strong_count_.compare_exchange_weak(current, current - 1,
                                              std::memory_order_acq_rel,
                                              std::memory_order_relaxed)) {
        if (current == 1) {
          tryReleasePayload();
          return true;
        }
        return false;
      }
    }
    return false;
  }

  std::uint32_t count() const noexcept {
    return strong_count_.load(std::memory_order_acquire);
  }

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

  bool tryPromote() noexcept {
    std::uint32_t current = strong_count_.load(std::memory_order_acquire);
    while (current > 0) {
      if (strong_count_.compare_exchange_weak(current, current + 1,
                                              std::memory_order_acquire,
                                              std::memory_order_relaxed)) {
        return true;
      }
    }
    return false;
  }

  bool canTeardown() const noexcept { return count() == 0; }
  bool canShutdown() const noexcept { return count() == 0 && weakCount() == 0; }

private:
  void tryReleasePayload() noexcept {
    if (payload_pool_ != nullptr && payload_handle_.isValid()) {
      if (payload_pool_->release(payload_handle_)) {
        clearPayload();
      }
    }
  }

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

  std::atomic<std::uint32_t> strong_count_{0};
  std::atomic<std::uint32_t> weak_count_{0};
  Handle payload_handle_{Handle::invalid()};
  Payload *payload_ptr_{nullptr};
  Pool *payload_pool_{nullptr};
};

// Specialization for legacy Slot-based control blocks.
template <typename SlotT>
class WeakSharedControlBlock<SlotT, void, void> {
  static_assert(SlotConcept<SlotT>,
                "WeakSharedControlBlock<SlotT> requires SlotConcept");

public:
  using Category = lease_category::WeakShared;
  using Slot = SlotT;
  using Payload = typename SlotT::Payload;

  WeakSharedControlBlock() = default;
  WeakSharedControlBlock(const WeakSharedControlBlock &) = delete;
  WeakSharedControlBlock &operator=(const WeakSharedControlBlock &) = delete;

  WeakSharedControlBlock(WeakSharedControlBlock &&other) noexcept
      : slot_(std::move(other.slot_)) {
    strong_count_.store(other.strong_count_.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
    weak_count_.store(other.weak_count_.load(std::memory_order_relaxed),
                      std::memory_order_relaxed);
  }

  WeakSharedControlBlock &operator=(WeakSharedControlBlock &&other) noexcept {
    if (this != &other) {
      strong_count_.store(other.strong_count_.load(std::memory_order_relaxed),
                          std::memory_order_relaxed);
      weak_count_.store(other.weak_count_.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
      slot_ = std::move(other.slot_);
    }
    return *this;
  }

  template <typename CreateFn>
    requires std::invocable<CreateFn, Payload &> &&
             std::convertible_to<std::invoke_result_t<CreateFn, Payload &>,
                                 bool>
  bool acquire(CreateFn &&createFn) noexcept {
    if (!slot_.create(std::forward<CreateFn>(createFn))) {
      return false;
    }
    strong_count_.fetch_add(1, std::memory_order_relaxed);
    return true;
  }

  bool release() noexcept {
    auto current = strong_count_.load(std::memory_order_acquire);
    while (current > 0) {
      if (strong_count_.compare_exchange_weak(current, current - 1,
                                              std::memory_order_acq_rel,
                                              std::memory_order_relaxed)) {
        if (current == 1) {
          if constexpr (SlotT::has_generation) {
            slot_.incrementGeneration();
          }
          if (weak_count_.load(std::memory_order_acquire) == 0) {
            return true;
          }
        }
        return false;
      }
    }
    return false;
  }

  template <typename DestroyFn>
    requires std::invocable<DestroyFn, Payload &>
  bool releaseAndDestroy(DestroyFn &&destroyFn) {
    if (strong_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      slot_.destroy(std::forward<DestroyFn>(destroyFn));
      if constexpr (SlotT::has_generation) {
        slot_.incrementGeneration();
      }
      if (weak_count_.load(std::memory_order_acquire) == 0) {
        return true;
      }
    }
    return false;
  }

  bool canTeardown() const noexcept { return count() == 0; }
  bool canShutdown() const noexcept { return count() == 0 && weakCount() == 0; }

  std::uint32_t count() const noexcept {
    return strong_count_.load(std::memory_order_acquire);
  }

  void acquireWeak() noexcept {
    weak_count_.fetch_add(1, std::memory_order_relaxed);
  }

  bool releaseWeak() noexcept {
    auto current = weak_count_.load(std::memory_order_acquire);
    while (current > 0) {
      if (weak_count_.compare_exchange_weak(current, current - 1,
                                            std::memory_order_acq_rel,
                                            std::memory_order_relaxed)) {
        return current == 1 &&
               strong_count_.load(std::memory_order_acquire) == 0;
      }
    }
    return false;
  }

  bool tryPromote() noexcept {
    if (!slot_.isCreated()) {
      return false;
    }
    std::uint32_t current = strong_count_.load(std::memory_order_acquire);
    while (current > 0) {
      if (strong_count_.compare_exchange_weak(current, current + 1,
                                              std::memory_order_acquire,
                                              std::memory_order_relaxed)) {
        return true;
      }
    }
    return false;
  }

  Payload &payload() noexcept { return slot_.get(); }
  const Payload &payload() const noexcept { return slot_.get(); }

  auto generation() const noexcept { return slot_.generation(); }

  std::uint32_t weakCount() const noexcept {
    return weak_count_.load(std::memory_order_acquire);
  }

  bool isCreated() const noexcept { return slot_.isCreated(); }

  template <typename Factory>
    requires std::invocable<Factory, Payload &>
  auto create(Factory &&factory) -> decltype(auto) {
    return slot_.create(std::forward<Factory>(factory));
  }

  template <typename Destructor>
    requires std::invocable<Destructor, Payload &>
  void destroy(Destructor &&destructor) {
    slot_.destroy(std::forward<Destructor>(destructor));
  }

private:
  std::atomic<std::uint32_t> strong_count_{0};
  std::atomic<std::uint32_t> weak_count_{0};
  SlotT slot_{};
};

} // namespace orteaf::internal::runtime::base

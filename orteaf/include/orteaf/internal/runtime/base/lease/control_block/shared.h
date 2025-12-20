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
class SharedControlBlock {
public:
  using Category = lease_category::Shared;
  using Handle = HandleT;
  using Payload = PayloadT;
  using Pool = PoolT;

  SharedControlBlock() = default;
  SharedControlBlock(const SharedControlBlock &) = delete;
  SharedControlBlock &operator=(const SharedControlBlock &) = delete;
  SharedControlBlock(SharedControlBlock &&) = default;
  SharedControlBlock &operator=(SharedControlBlock &&) = default;
  ~SharedControlBlock() = default;

  // Payload binding
  bool canBindPayload() const noexcept {
    return payload_ptr_ == nullptr && count() == 0;
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

  bool canTeardown() const noexcept { return count() == 0; }
  bool canShutdown() const noexcept { return count() == 0; }

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
  Handle payload_handle_{Handle::invalid()};
  Payload *payload_ptr_{nullptr};
  Pool *payload_pool_{nullptr};
};

// Specialization for legacy Slot-based control blocks.
template <typename SlotT>
class SharedControlBlock<SlotT, void, void> {
  static_assert(SlotConcept<SlotT>,
                "SharedControlBlock<SlotT> requires SlotConcept");

public:
  using Category = lease_category::Shared;
  using Slot = SlotT;
  using Payload = typename SlotT::Payload;

  SharedControlBlock() = default;
  SharedControlBlock(const SharedControlBlock &) = delete;
  SharedControlBlock &operator=(const SharedControlBlock &) = delete;

  SharedControlBlock(SharedControlBlock &&other) noexcept
      : slot_(std::move(other.slot_)) {
    strong_count_.store(other.strong_count_.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
  }

  SharedControlBlock &operator=(SharedControlBlock &&other) noexcept {
    if (this != &other) {
      strong_count_.store(other.strong_count_.load(std::memory_order_relaxed),
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
          return true;
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
      bool destroyed = slot_.destroy(std::forward<DestroyFn>(destroyFn));
      if constexpr (SlotT::has_generation) {
        slot_.incrementGeneration();
      }
      return destroyed;
    }
    return false;
  }

  bool canTeardown() const noexcept { return count() == 0; }
  bool canShutdown() const noexcept { return count() == 0; }

  std::uint32_t count() const noexcept {
    return strong_count_.load(std::memory_order_acquire);
  }

  Payload &payload() noexcept { return slot_.get(); }
  const Payload &payload() const noexcept { return slot_.get(); }

  auto generation() const noexcept { return slot_.generation(); }

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
  SlotT slot_{};
};

} // namespace orteaf::internal::runtime::base

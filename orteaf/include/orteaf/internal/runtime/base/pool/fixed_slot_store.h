#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/base/runtime_block_vector.h"
#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::runtime::base::pool {

/**
 * @brief Fixed array storage without freelist reuse.
 *
 * FixedSlotStore provides a pool-like API but does not reuse slots via a
 * freelist. Slots are addressed directly by handle index, and
 * acquire/tryAcquire simply validate that the requested slot is already
 * created.
 *
 * This is useful for payloads whose lifetime is tied to external systems
 * (device objects, global buffers, etc.) where pool reuse is unnecessary or
 * undesirable, but access through handles should still be validated.
 *
 * Generation tracking is supported when Handle::has_generation is true. In
 * that case, emplace can update the generation value for the handle's index.
 *
 * @tparam Traits Policy type defining Payload/Handle/Request/Context/Config and
 *         creation/destruction hooks.
 */
template <typename Traits> class FixedSlotStore {
public:
  using Payload = typename Traits::Payload;
  using Handle = typename Traits::Handle;
  using Request = typename Traits::Request;
  using Context = typename Traits::Context;
  using Config = typename Traits::Config;

  /**
   * @brief Lightweight handle+pointer pair returned by acquisition calls.
   *
   * SlotRef is a non-owning view. It is only valid if the underlying handle is
   * valid and the payload is created.
   */
  struct SlotRef {
    Handle handle{Handle::invalid()};
    Payload *payload_ptr{nullptr};

    /**
     * @brief Returns true if the handle is valid and the pointer is non-null.
     */
    bool valid() const noexcept {
      return handle.isValid() && payload_ptr != nullptr;
    }
  };

  FixedSlotStore() = default;
  FixedSlotStore(const FixedSlotStore &) = delete;
  FixedSlotStore &operator=(const FixedSlotStore &) = delete;
  FixedSlotStore(FixedSlotStore &&) = default;
  FixedSlotStore &operator=(FixedSlotStore &&) = default;
  ~FixedSlotStore() = default;

  /**
   * @brief Applies configuration to the store, growing storage if needed.
   *
   * This method does not reset existing state. Call shutdown() first when
   * reinitialization is required. Configuration changes are restricted to
   * capacity growth and a fixed block size.
   *
   * @param config Configuration containing capacity and block size.
   * @return The previous capacity before applying the configuration.
   * @throws OrteafErrc::InvalidArgument if capacity exceeds handle range.
   */
  std::size_t configure(const Config &config) { return applyConfig(config); }

  /**
   * @brief Returns the number of slots in the store.
   */
  std::size_t capacity() const noexcept { return payloads_.size(); }

  /**
   * @brief Destroys all created payloads and releases storage.
   *
   * This method iterates over all slots and calls Traits::destroy for any
   * slot marked as created before clearing internal storage. Callers should
   * verify canShutdown at the manager layer before calling this method.
   *
   * @param request Request details forwarded to Traits::destroy.
   * @param context Context details forwarded to Traits::destroy.
   */
  void shutdown(const Request &request = {},
                const Context &context = {}) noexcept {
    // Destroy all created payloads before clearing storage
    for (std::size_t idx = 0; idx < payloads_.size(); ++idx) {
      if (created_[idx] != 0) {
        Handle handle = makeHandle(static_cast<index_type>(idx));
        destroy(handle, request, context);
      }
    }
    payloads_.clear();
    generations_.clear();
    created_.clear();
  }

  /**
   * @brief Creates payloads for all slots in the store.
   *
   * @param request Request details forwarded to Traits::create.
   * @param context Context details forwarded to Traits::create.
   * @return True if all payloads were created successfully.
   */
  bool createAll(const Request &request, const Context &context) {
    return createRange(0, payloads_.size(), request, context);
  }

  /**
   * @brief Creates payloads for a slot range [start, end).
   *
   * @param start Inclusive start index.
   * @param end Exclusive end index.
   * @param request Request details forwarded to Traits::create.
   * @param context Context details forwarded to Traits::create.
   * @return True if all payloads were created successfully.
   * @throws OrteafErrc::InvalidArgument if range is invalid.
   */
  bool createRange(std::size_t start, std::size_t end,
                   const Request &request, const Context &context) {
    if (start > end || end > payloads_.size()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          "FixedSlotStore create range is out of bounds");
    }
    bool all_created = true;
    for (std::size_t idx = start; idx < end; ++idx) {
      Request slot_request = request;
      setHandleIfPresent(slot_request,
                         makeHandle(static_cast<index_type>(idx)));
      if (!emplace(makeHandle(static_cast<index_type>(idx)), slot_request,
                   context)) {
        all_created = false;
      }
    }
    return all_created;
  }

  /**
   * @brief Acquires a slot by handle, or throws if the slot is not created.
   *
   * acquire expects the Request to carry the target handle. The returned
   * SlotRef is valid only if that handle is valid and already created.
   *
   * @throws OrteafErrc::OutOfRange if the slot is unavailable.
   */
  SlotRef acquire(const Request &request, const Context &context) {
    SlotRef ref = tryAcquire(request, context);
    if (!ref.valid()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::OutOfRange,
          "FixedSlotStore slot is not available");
    }
    return ref;
  }

  /**
   * @brief Attempts to acquire a slot by handle without throwing.
   *
   * @return SlotRef with invalid handle and null pointer if not available.
   */
  SlotRef tryAcquire(const Request &request, const Context &) noexcept {
    Handle handle = request.handle;
    if (!isValid(handle) || !isCreated(handle)) {
      return SlotRef{Handle::invalid(), nullptr};
    }
    return SlotRef{handle, &payloads_[static_cast<std::size_t>(handle.index)]};
  }

  /**
   * @brief Release is a no-op for FixedSlotStore.
   *
   * This method is provided for API symmetry with SlotPool. It only validates
   * the handle and returns whether it is in range (and generation matches).
   */
  bool release(Handle handle) noexcept {
    if constexpr (destroy_on_release_) {
      static_assert(std::is_default_constructible_v<Request>,
                    "FixedSlotStore::release(handle) requires "
                    "default-constructible Request when destroy_on_release is "
                    "enabled");
      static_assert(std::is_default_constructible_v<Context>,
                    "FixedSlotStore::release(handle) requires "
                    "default-constructible Context when destroy_on_release is "
                    "enabled");
      return release(handle, Request{}, Context{});
    }
    return isValid(handle);
  }

  /**
   * @brief Releases a slot and applies destroy policy if enabled.
   *
   * When Traits::destroy_on_release is true, release destroys the payload
   * (if created). No freelist reuse is performed.
   */
  bool release(Handle handle, const Request &request,
               const Context &context) noexcept {
    if (!isValid(handle)) {
      return false;
    }
    if constexpr (destroy_on_release_) {
      if (!isCreated(handle)) {
        return false;
      }
      if (!destroy(handle, request, context)) {
        return false;
      }
    }
    return true;
  }

  /**
   * @brief Returns a pointer to the payload if valid and created.
   */
  Payload *get(Handle handle) noexcept {
    if (!isValid(handle) || !isCreated(handle)) {
      return nullptr;
    }
    return &payloads_[static_cast<std::size_t>(handle.index)];
  }

  /**
   * @brief Const overload of get().
   */
  const Payload *get(Handle handle) const noexcept {
    if (!isValid(handle) || !isCreated(handle)) {
      return nullptr;
    }
    return &payloads_[static_cast<std::size_t>(handle.index)];
  }

  /**
   * @brief Validates a handle against bounds and generation (if supported).
   */
  bool isValid(Handle handle) const noexcept {
    const auto idx = static_cast<std::size_t>(handle.index);
    if (idx >= payloads_.size()) {
      return false;
    }
    if constexpr (Handle::has_generation) {
      return handle.generation == generations_[idx];
    }
    return true;
  }

  /**
   * @brief Returns whether a payload has been created for the given handle.
   */
  bool isCreated(Handle handle) const noexcept {
    if (!isValid(handle)) {
      return false;
    }
    return created_[static_cast<std::size_t>(handle.index)] != 0;
  }

  /**
   * @brief Creates a payload using Traits::create at a specific handle.
   *
   * The handle must be valid and not already created. If generation is used,
   * this updates the stored generation for the handle's index.
   *
   * @return True if creation succeeded.
   */
  bool emplace(Handle handle, const Request &request, const Context &context) {
    if (!isValid(handle) || isCreated(handle)) {
      return false;
    }
    const std::size_t idx = static_cast<std::size_t>(handle.index);
    if constexpr (Handle::has_generation) {
      generations_[idx] = handle.generation;
    }
    auto &payload = payloads_[idx];
    const bool created = Traits::create(payload, request, context);
    if (created) {
      setCreated(handle, true);
    }
    return created;
  }

  /**
   * @brief Creates a payload using a caller-provided factory.
   *
   * This overload allows per-call customization without changing Traits.
   * The provided function must be invocable as
   *   bool(Payload&, const Request&, const Context&).
   *
   * @return True if creation succeeded.
   */
  template <typename CreateFn>
    requires std::invocable<CreateFn, Payload &, const Request &,
                            const Context &> &&
             std::convertible_to<
                 std::invoke_result_t<CreateFn, Payload &, const Request &,
                                      const Context &>,
                 bool>
  bool emplace(Handle handle, const Request &request, const Context &context,
               CreateFn &&createFn) {
    if (!isValid(handle) || isCreated(handle)) {
      return false;
    }
    const std::size_t idx = static_cast<std::size_t>(handle.index);
    if constexpr (Handle::has_generation) {
      generations_[idx] = handle.generation;
    }
    auto &payload = payloads_[idx];
    const bool created =
        std::forward<CreateFn>(createFn)(payload, request, context);
    if (created) {
      setCreated(handle, true);
    }
    return created;
  }

  /**
   * @brief Destroys a payload using Traits::destroy.
   *
   * The handle must be valid and already created. After destruction, the
   * created flag is cleared.
   *
   * @return True if destruction proceeded.
   */
  bool destroy(Handle handle, const Request &request, const Context &context) {
    if (!isValid(handle) || !isCreated(handle)) {
      return false;
    }
    auto &payload = payloads_[static_cast<std::size_t>(handle.index)];
    Traits::destroy(payload, request, context);
    setCreated(handle, false);
    return true;
  }

  /**
   * @brief Destroys a payload using a caller-provided function.
   *
   * The function may return void or bool. If it returns bool and returns false,
   * destroy is treated as failed and the created flag remains set.
   *
   * @return True if destruction proceeded and the created flag was cleared.
   */
  template <typename DestroyFn>
    requires std::invocable<DestroyFn, Payload &, const Request &,
                            const Context &>
  bool destroy(Handle handle, const Request &request, const Context &context,
               DestroyFn &&destroyFn) {
    if (!isValid(handle) || !isCreated(handle)) {
      return false;
    }
    auto &payload = payloads_[static_cast<std::size_t>(handle.index)];
    if constexpr (std::convertible_to<
                      std::invoke_result_t<DestroyFn, Payload &,
                                           const Request &, const Context &>,
                      bool>) {
      if (!std::forward<DestroyFn>(destroyFn)(payload, request, context)) {
        return false;
      }
    } else {
      std::forward<DestroyFn>(destroyFn)(payload, request, context);
    }
    setCreated(handle, false);
    return true;
  }

private:
  using index_type = typename Handle::index_type;
  using generation_storage_t =
      std::conditional_t<Handle::has_generation,
                         typename Handle::generation_type, std::uint8_t>;
  static constexpr bool destroy_on_release_ = [] {
    if constexpr (requires { Traits::destroy_on_release; }) {
      return static_cast<bool>(Traits::destroy_on_release);
    }
    return false;
  }();

  static void setHandleIfPresent(Request &request, Handle handle) noexcept {
    if constexpr (requires { request.handle = handle; }) {
      request.handle = handle;
    }
  }

  Handle makeHandle(index_type idx) const noexcept {
    if constexpr (Handle::has_generation) {
      return Handle{idx, generations_[static_cast<std::size_t>(idx)]};
    }
    return Handle{idx};
  }

  std::size_t applyConfig(const Config &config) {
    const std::size_t capacity = config.capacity;
    if (capacity > static_cast<std::size_t>(Handle::invalid_index())) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          "FixedSlotStore capacity exceeds handle range");
    }
    const std::size_t desired_block_size = config.block_size;
    if (desired_block_size == 0 && capacity != 0) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          "FixedSlotStore requires non-zero block size when capacity > 0");
    }
    if (desired_block_size == 0) {
      if (payloads_.blockSize() != 0) {
        ::orteaf::internal::diagnostics::error::throwError(
            ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
            "FixedSlotStore block size cannot be reset to zero");
      }
    } else if (payloads_.blockSize() != desired_block_size) {
      payloads_.resizeBlocks(desired_block_size);
    }
    const std::size_t old_capacity = payloads_.size();
    if (capacity <= old_capacity) {
      return old_capacity;
    }
    payloads_.resize(capacity);
    generations_.resize(capacity, 0);
    created_.resize(capacity, 0);
    return old_capacity;
  }

  void setCreated(Handle handle, bool created) noexcept {
    if (!isValid(handle)) {
      return;
    }
    created_[static_cast<std::size_t>(handle.index)] = created ? 1 : 0;
  }

  ::orteaf::internal::base::RuntimeBlockVector<Payload> payloads_{};
  ::orteaf::internal::base::HeapVector<generation_storage_t> generations_{};
  ::orteaf::internal::base::HeapVector<std::uint8_t> created_{};
};

} // namespace orteaf::internal::runtime::base::pool

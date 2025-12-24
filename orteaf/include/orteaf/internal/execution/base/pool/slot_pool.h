#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/base/runtime_block_vector.h"
#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::execution::base::pool {

/**
 * @brief Slot-based pool with freelist reuse and optional generation tracking.
 *
 * SlotPool stores a contiguous array of Payload objects. Acquisition returns a
 * SlotRef containing a handle and a pointer to the payload storage. Released
 * slots are pushed back to a freelist and can be reacquired later. If the
 * handle type supports generation tracking (Handle::has_generation), releases
 * bump the generation to invalidate stale handles.
 *
 * Creation and destruction of payloads is separated from slot acquisition. This
 * allows the pool to manage storage reuse independently of object lifetime.
 * - reserve/tryReserveUncreated: reserve an uncreated slot for initialization.
 * - acquire/tryAcquireCreated: acquire an already-created slot for reuse.
 * - emplace/destroy: create/destroy the payload for a specific handle.
 *
 * The pool does not interpret Request/Context beyond passing them to Traits.
 * This keeps pool APIs stable while allowing callers to define allocation
 * policies and initialization behavior in Traits or custom lambdas.
 *
 * @tparam Traits Policy type defining Payload/Handle/Request/Context and
 *         creation/destruction hooks.
 */
template <typename Traits> class SlotPool {
public:
  using Payload = typename Traits::Payload;
  using Handle = typename Traits::Handle;
  using Request = typename Traits::Request;
  using Context = typename Traits::Context;

  struct Config {
    std::size_t size{0};
    std::size_t block_size{0};
  };

  /**
   * @brief Lightweight handle+pointer pair returned by acquisition calls.
   *
   * SlotRef is a non-owning view. It does not guarantee that the payload is
   * created; callers should call emplace (or use a manager that does) before
   * accessing the payload contents.
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

  SlotPool() = default;
  SlotPool(const SlotPool &) = delete;
  SlotPool &operator=(const SlotPool &) = delete;
  SlotPool(SlotPool &&) = default;
  SlotPool &operator=(SlotPool &&) = default;
  ~SlotPool() = default;

  /**
   * @brief Applies configuration to the pool, growing storage if needed.
   *
   * This method does not reset existing state. Call shutdown() first when
   * reinitialization is required. Configuration changes are restricted to
   * size growth and a fixed block size.
   *
   * @param config Configuration containing size and block size.
   * @return The previous size before applying the configuration.
   * @throws OrteafErrc::InvalidArgument if size exceeds handle range.
   */
  std::size_t configure(const Config &config) { return applyConfig(config); }

  /**
   * @brief Returns the number of slots in the pool.
   */
  std::size_t size() const noexcept { return payloads_.size(); }
  /**
   * @brief Returns the reserved storage capacity in slots.
   */
  std::size_t capacity() const noexcept { return payloads_.capacity(); }
  /**
   * @brief Returns the payload block size in use.
   */
  std::size_t blockSize() const noexcept { return payloads_.blockSize(); }
  /**
   * @brief Returns the number of slots currently available in the freelist.
   */
  std::size_t available() const noexcept { return freelist_.size(); }

  /**
   * @brief Reserves storage for at least new_capacity slots.
   */
  void reserve(std::size_t new_capacity) { reserveStorage(new_capacity); }

  /**
   * @brief Resizes the pool to new_size slots, growing only.
   *
   * @return The previous size before resizing.
   * @throws OrteafErrc::InvalidArgument if new_size is smaller than current.
   */
  std::size_t resize(std::size_t new_size) { return resizeStorage(new_size); }

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
    for (std::size_t idx = 0; idx < size(); ++idx) {
      if (created_[idx] != 0) {
        Handle handle = makeHandle(static_cast<index_type>(idx));
        destroy(handle, request, context);
      }
    }
    payloads_.clear();
    generations_.clear();
    created_.clear();
    freelist_.clear();
  }

  /**
   * @brief Creates payloads for all slots in the pool.
   *
   * @param request Request details forwarded to Traits::create.
   * @param context Context details forwarded to Traits::create.
   * @return True if all payloads were created successfully.
   */
  bool createAll(const Request &request, const Context &context) {
    return createRange(0, size(), request, context);
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
  bool createRange(std::size_t start, std::size_t end, const Request &request,
                   const Context &context) {
    if (start > end || end > size()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          "SlotPool create range is out of bounds");
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
   * @brief Acquires a created slot or throws if none are available.
   *
   * tryAcquireCreated returns only slots with isCreated=true. It does not
   * create payloads itself.
   *
   * @param request Allocation/request details passed through for symmetry.
   * @param context Context information (backend/device, etc.).
   * @return SlotRef with a valid handle and payload pointer.
   * @throws OrteafErrc::OutOfRange if the freelist is empty.
   */
  SlotRef acquireCreated(const Request &request, const Context &context) {
    SlotRef ref = tryAcquireCreated(request, context);
    if (!ref.valid()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::OutOfRange,
          "SlotPool is empty");
    }
    return ref;
  }

  /**
   * @brief Attempts to acquire a created slot without throwing.
   *
   * @return SlotRef with invalid handle and null pointer if no slots available.
   */
  SlotRef tryAcquireCreated(const Request &, const Context &) noexcept {
    if (freelist_.empty()) {
      return SlotRef{Handle::invalid(), nullptr};
    }
    const std::size_t scan_count = freelist_.size();
    for (std::size_t i = 0; i < scan_count; ++i) {
      const index_type idx = freelist_.back();
      freelist_.resize(freelist_.size() - 1);
      if (created_[static_cast<std::size_t>(idx)] != 0) {
        return SlotRef{makeHandle(idx), &payloads_[idx]};
      }
      freelist_.pushBack(idx);
    }
    return SlotRef{Handle::invalid(), nullptr};
  }

  /**
   * @brief Reserves an uncreated slot or throws if none are available.
   *
   * tryReserveUncreated returns only slots with isCreated=false. It does not
   * create payloads itself. Use emplace to initialize the payload.
   *
   * @param request Allocation/request details passed through for symmetry.
   * @param context Context information (backend/device, etc.).
   * @return SlotRef with a valid handle and payload pointer.
   * @throws OrteafErrc::OutOfRange if no uncreated slots are available.
   */
  SlotRef reserveUncreated(const Request &request, const Context &context) {
    SlotRef ref = tryReserveUncreated(request, context);
    if (!ref.valid()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::OutOfRange,
          "SlotPool is empty");
    }
    return ref;
  }

  /**
   * @brief Attempts to reserve an uncreated slot without throwing.
   *
   * @return SlotRef with invalid handle and null pointer if none available.
   */
  SlotRef tryReserveUncreated(const Request &, const Context &) noexcept {
    if (freelist_.empty()) {
      return SlotRef{Handle::invalid(), nullptr};
    }
    const std::size_t scan_count = freelist_.size();
    for (std::size_t i = 0; i < scan_count; ++i) {
      const index_type idx = freelist_.back();
      freelist_.resize(freelist_.size() - 1);
      if (created_[static_cast<std::size_t>(idx)] == 0) {
        return SlotRef{makeHandle(idx), &payloads_[idx]};
      }
      freelist_.pushBack(idx);
    }
    return SlotRef{Handle::invalid(), nullptr};
  }

  /**
   * @brief Releases a slot back to the freelist.
   *
   * If Handle::has_generation is true, generation is incremented to invalidate
   * previously acquired handles. The pool does not destroy payloads; call
   * destroy first if needed.
   *
   * @param handle Slot handle to release.
   * @return True if the handle was valid and release occurred.
   */
  bool release(Handle handle) noexcept {
    if constexpr (destroy_on_release_) {
      static_assert(std::is_default_constructible_v<Request>,
                    "SlotPool::release(handle) requires default-constructible "
                    "Request when destroy_on_release is enabled");
      static_assert(std::is_default_constructible_v<Context>,
                    "SlotPool::release(handle) requires default-constructible "
                    "Context when destroy_on_release is enabled");
      return release(handle, Request{}, Context{});
    }
    return releaseImpl(handle);
  }

  /**
   * @brief Releases a slot and applies destroy policy if enabled.
   *
   * When Traits::destroy_on_release is true, release destroys the payload
   * (if created) before returning the slot to the freelist.
   *
   * @param handle Slot handle to release.
   * @param request Request details for destroy (if used by Traits).
   * @param context Context details for destroy (if used by Traits).
   * @return True if the handle was valid and release occurred.
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
    return releaseImpl(handle);
  }

  /**
   * @brief Returns a pointer to the payload storage if the handle is valid.
   *
   * This does not check created state; callers may use isCreated() if they
   * need to distinguish constructed vs. unconstructed payloads.
   */
  Payload *get(Handle handle) noexcept {
    if (!isValid(handle)) {
      return nullptr;
    }
    return &payloads_[static_cast<std::size_t>(handle.index)];
  }

  /**
   * @brief Const overload of get().
   */
  const Payload *get(Handle handle) const noexcept {
    if (!isValid(handle)) {
      return nullptr;
    }
    return &payloads_[static_cast<std::size_t>(handle.index)];
  }

  /**
   * @brief Invoke a callable for each created payload.
   *
   * This scans all slots and invokes the callable for slots marked as created.
   * Intended for pool-wide inspection without exposing raw index accessors.
   */
  template <typename Func>
    requires std::invocable<Func, std::size_t, const Payload &>
  void forEachCreated(Func &&func) const {
    const std::size_t count = size();
    for (std::size_t idx = 0; idx < count; ++idx) {
      if (created_[idx] != 0) {
        std::forward<Func>(func)(idx, payloads_[idx]);
      }
    }
  }

  /**
   * @brief Validates a handle against bounds and generation (if supported).
   *
   * @return True if index is in range and generation matches (when enabled).
   */
  bool isValid(Handle handle) const noexcept {
    const auto idx = static_cast<std::size_t>(handle.index);
    if (idx >= size()) {
      return false;
    }
    if constexpr (Handle::has_generation) {
      return handle.generation == generations_[idx];
    }
    return true;
  }

  /**
   * @brief Returns whether a payload has been created for the given handle.
   *
   * The created flag is orthogonal to handle validity; invalid handles return
   * false. Creation is controlled by emplace/destroy or explicit setCreated.
   */
  bool isCreated(Handle handle) const noexcept {
    if (!isValid(handle)) {
      return false;
    }
    return created_[static_cast<std::size_t>(handle.index)] != 0;
  }

  /**
   * @brief Creates a payload using Traits::create.
   *
   * The handle must be valid and not already created. Traits::create must
   * return a bool indicating success. On success, the created flag is set.
   *
   * @return True if creation succeeded.
   */
  bool emplace(Handle handle, const Request &request, const Context &context) {
    if (!isValid(handle) || isCreated(handle)) {
      return false;
    }
    auto &payload = payloads_[static_cast<std::size_t>(handle.index)];
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
    auto &payload = payloads_[static_cast<std::size_t>(handle.index)];
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

  std::size_t applyConfig(const Config &config) {
    const std::size_t desired_size = config.size;
    if (desired_size > static_cast<std::size_t>(Handle::invalid_index())) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          "SlotPool size exceeds handle range");
    }
    const std::size_t desired_block_size = config.block_size;
    if (desired_block_size == 0) {
      if (desired_size != 0 && payloads_.blockSize() == 0) {
        ::orteaf::internal::diagnostics::error::throwError(
            ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
            "SlotPool requires non-zero block size when size > 0");
      }
    } else if (payloads_.blockSize() != desired_block_size) {
      payloads_.resizeBlocks(desired_block_size);
    }
    return resizeStorage(desired_size);
  }

  void reserveStorage(std::size_t new_capacity) {
    if (new_capacity > static_cast<std::size_t>(Handle::invalid_index())) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          "SlotPool capacity exceeds handle range");
    }
    payloads_.reserve(new_capacity);
    generations_.reserve(new_capacity);
    created_.reserve(new_capacity);
    freelist_.reserve(new_capacity);
  }

  std::size_t resizeStorage(std::size_t new_size) {
    const std::size_t old_size = size();
    if (new_size < old_size) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          "SlotPool size cannot shrink without shutdown");
    }
    if (new_size > static_cast<std::size_t>(Handle::invalid_index())) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          "SlotPool size exceeds handle range");
    }
    if (new_size == old_size) {
      return old_size;
    }
    payloads_.resize(new_size);
    generations_.resize(new_size, 0);
    created_.resize(new_size, 0);
    freelist_.reserve(new_size);
    for (std::size_t i = new_size; i > old_size; --i) {
      freelist_.pushBack(static_cast<index_type>(i - 1));
    }
    return old_size;
  }

  bool releaseImpl(Handle handle) noexcept {
    if (!isValid(handle)) {
      return false;
    }
    const std::size_t idx = static_cast<std::size_t>(handle.index);
    if constexpr (Handle::has_generation) {
      ++generations_[idx];
    }
    freelist_.pushBack(static_cast<index_type>(idx));
    return true;
  }

  void setCreated(Handle handle, bool created) noexcept {
    if (!isValid(handle)) {
      return;
    }
    created_[static_cast<std::size_t>(handle.index)] = created ? 1 : 0;
  }

  Handle makeHandle(index_type idx) const noexcept {
    if constexpr (Handle::has_generation) {
      return Handle{idx, generations_[static_cast<std::size_t>(idx)]};
    }
    return Handle{idx};
  }

  ::orteaf::internal::base::RuntimeBlockVector<Payload> payloads_{};
  ::orteaf::internal::base::HeapVector<generation_storage_t> generations_{};
  ::orteaf::internal::base::HeapVector<std::uint8_t> created_{};
  ::orteaf::internal::base::HeapVector<index_type> freelist_{};
};

} // namespace orteaf::internal::execution::base::pool

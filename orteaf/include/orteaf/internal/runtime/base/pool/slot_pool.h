#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::runtime::base::pool {

/**
 * @brief Slot-based pool with freelist reuse and optional generation tracking.
 *
 * SlotPool stores a contiguous array of Payload objects. Acquisition returns a
 * SlotRef containing a handle and a pointer to the payload storage. Released
 * slots are pushed back to a freelist and can be reacquired later. If the handle
 * type supports generation tracking (Handle::has_generation), releases bump the
 * generation to invalidate stale handles.
 *
 * Creation and destruction of payloads is separated from slot acquisition. This
 * allows the pool to manage storage reuse independently of object lifetime.
 * - reserve/tryReserve: reserve an uncreated slot for initialization.
 * - acquire/tryAcquire: acquire an already-created slot for reuse.
 * - emplace/destroy: create/destroy the payload for a specific handle.
 *
 * The pool does not interpret Request/Context beyond passing them to Traits.
 * This keeps pool APIs stable while allowing callers to define allocation
 * policies and initialization behavior in Traits or custom lambdas.
 *
 * @tparam Traits Policy type defining Payload/Handle/Request/Context/Config and
 *         creation/destruction hooks.
 */
template <typename Traits> class SlotPool {
public:
  using Payload = typename Traits::Payload;
  using Handle = typename Traits::Handle;
  using Request = typename Traits::Request;
  using Context = typename Traits::Context;
  using Config = typename Traits::Config;

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
   * @brief Initializes storage and freelist using Config capacity.
   *
   * This method resets the internal state, resizes payload storage, initializes
   * generation and created flags, and fills the freelist with all slot indices.
   *
   * @param config Configuration containing capacity.
   * @throws OrteafErrc::InvalidArgument if capacity exceeds handle range.
   */
  void initialize(const Config &config) {
    shutdown();
    grow(config);
  }

  /**
   * @brief Initializes storage and creates payloads for all slots.
   *
   * This method resets the internal state, grows storage to capacity, and
   * creates payloads for all slots using Traits::create.
   *
   * @param config Configuration containing capacity.
   * @param request Request details forwarded to Traits::create.
   * @param context Context details forwarded to Traits::create.
   * @return True if all payloads were created successfully.
   */
  bool initializeAndCreate(const Config &config, const Request &request,
                           const Context &context) {
    shutdown();
    return growAndCreate(config, request, context);
  }

  /**
   * @brief Returns the number of slots in the pool.
   */
  std::size_t capacity() const noexcept { return payloads_.size(); }
  /**
   * @brief Returns the number of slots currently available in the freelist.
   */
  std::size_t available() const noexcept { return freelist_.size(); }

  /**
   * @brief Releases all storage and resets internal state.
   */
  void shutdown() noexcept {
    payloads_.clear();
    generations_.clear();
    created_.clear();
    freelist_.clear();
  }

  /**
   * @brief Grows storage to the specified capacity without creating payloads.
   *
   * New slots are appended to the freelist in "not created" state.
   *
   * @param config Configuration containing the new capacity.
   * @throws OrteafErrc::InvalidArgument if capacity exceeds handle range.
   */
  void grow(const Config &config) {
    growStorage(config);
  }

  /**
   * @brief Grows storage and creates payloads for new slots.
   *
   * New slots are created using Traits::create and remain available for
   * acquire (created=true). Returns false if any create fails.
   *
   * @param config Configuration containing the new capacity.
   * @param request Request details forwarded to Traits::create.
   * @param context Context details forwarded to Traits::create.
   * @return True if all new payloads were created successfully.
   * @throws OrteafErrc::InvalidArgument if capacity exceeds handle range.
   */
  bool growAndCreate(const Config &config, const Request &request,
                     const Context &context) {
    const std::size_t old_capacity = growStorage(config);
    const std::size_t new_capacity = payloads_.size();
    bool all_created = true;
    for (std::size_t idx = old_capacity; idx < new_capacity; ++idx) {
      Request slot_request = request;
      setHandleIfPresent(slot_request, makeHandle(static_cast<index_type>(idx)));
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
   * acquire returns only slots with isCreated=true. It does not create
   * payloads itself.
   *
   * @param request Allocation/request details passed through for symmetry.
   * @param context Context information (backend/device, etc.).
   * @return SlotRef with a valid handle and payload pointer.
   * @throws OrteafErrc::OutOfRange if the freelist is empty.
   */
  SlotRef acquire(const Request &request, const Context &context) {
    SlotRef ref = tryAcquire(request, context);
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
  SlotRef tryAcquire(const Request &, const Context &) noexcept {
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
   * reserve returns only slots with isCreated=false. It does not create
   * payloads itself. Use emplace to initialize the payload.
   *
   * @param request Allocation/request details passed through for symmetry.
   * @param context Context information (backend/device, etc.).
   * @return SlotRef with a valid handle and payload pointer.
   * @throws OrteafErrc::OutOfRange if no uncreated slots are available.
   */
  SlotRef reserve(const Request &request, const Context &context) {
    SlotRef ref = tryReserve(request, context);
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
  SlotRef tryReserve(const Request &, const Context &) noexcept {
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
   * @brief Validates a handle against bounds and generation (if supported).
   *
   * @return True if index is in range and generation matches (when enabled).
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
                      std::invoke_result_t<DestroyFn, Payload &, const Request &,
                                           const Context &>,
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
      std::conditional_t<Handle::has_generation, typename Handle::generation_type,
                         std::uint8_t>;
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

  std::size_t growStorage(const Config &config) {
    const std::size_t capacity = config.capacity;
    if (capacity > static_cast<std::size_t>(Handle::invalid_index())) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          "SlotPool capacity exceeds handle range");
    }
    const std::size_t old_capacity = payloads_.size();
    if (capacity <= old_capacity) {
      return old_capacity;
    }
    payloads_.resize(capacity);
    generations_.resize(capacity, 0);
    created_.resize(capacity, 0);
    freelist_.reserve(capacity);
    for (std::size_t i = capacity; i > old_capacity; --i) {
      freelist_.pushBack(static_cast<index_type>(i - 1));
    }
    return old_capacity;
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

  ::orteaf::internal::base::HeapVector<Payload> payloads_{};
  ::orteaf::internal::base::HeapVector<generation_storage_t> generations_{};
  ::orteaf::internal::base::HeapVector<std::uint8_t> created_{};
  ::orteaf::internal::base::HeapVector<index_type> freelist_{};
};

} // namespace orteaf::internal::runtime::base::pool

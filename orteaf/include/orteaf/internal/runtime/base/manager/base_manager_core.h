#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

#include <orteaf/internal/diagnostics/error/error.h>
#include <orteaf/internal/runtime/base/lease/concepts.h>

namespace orteaf::internal::runtime::base {

// =============================================================================
// Manager Traits Concept
// =============================================================================

/// @brief Concept for validating Manager Traits
/// Required members:
///   using ControlBlock = ...;  // Must satisfy ControlBlockConcept
///   using Handle = ...;        // Handle type with .index member
///   static constexpr const char* Name = "...";  // Manager name for errors
template <typename Traits>
concept ManagerTraitsConcept = requires {
  typename Traits::ControlBlock;
  typename Traits::Handle;
  { Traits::Name } -> std::convertible_to<const char *>;
  // Verify generation compatibility between Handle and ControlBlock::Slot
  requires Traits::Handle::has_generation ==
               Traits::ControlBlock::Slot::has_generation;
} && ControlBlockConcept<typename Traits::ControlBlock>;

// =============================================================================
// BaseManagerCore
// =============================================================================

/// @brief Base manager providing common infrastructure for resource management
/// @tparam Traits A traits struct satisfying ManagerTraitsConcept
template <typename Traits>
  requires ManagerTraitsConcept<Traits>
class BaseManagerCore {
protected:
  using ControlBlock = typename Traits::ControlBlock;
  using Handle = typename Traits::Handle;
  using IndexType = typename Handle::index_type;

  static constexpr const char *managerName() { return Traits::Name; }

  // =========================================================================
  // Initialization State
  // =========================================================================

  /// @brief Check if the manager is initialized
  bool isInitialized() const noexcept { return initialized_; }

  /// @brief Ensure manager is initialized, throw if not
  void ensureInitialized() const {
    if (!initialized_) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          std::string(managerName()) + " has not been initialized");
    }
  }

  /// @brief Ensure manager is not initialized, throw if already initialized
  void ensureNotInitialized() const {
    if (initialized_) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          std::string(managerName()) + " already initialized");
    }
  }

  // =========================================================================
  // Setup / Teardown
  // =========================================================================

  /// @brief Setup pool with capacity, calling createFn for each control block
  /// @tparam CreateFn Callable: bool(ControlBlock&, size_t index) - returns
  /// true to add to freelist
  /// @param capacity Number of control blocks to create
  /// @param createFn Factory function called for each control block
  template <typename CreateFn>
  void setupPool(std::size_t capacity, CreateFn &&createFn) {
    ensureNotInitialized();
    control_blocks_.resize(capacity);
    for (std::size_t i = 0; i < capacity; ++i) {
      if (createFn(control_blocks_[i], i)) {
        freelist_.push_back(static_cast<IndexType>(i));
      }
    }
    initialized_ = true;
  }

  /// @brief Setup pool with capacity, initializing control blocks to default
  /// @param capacity Number of control blocks to create
  void setupPool(std::size_t capacity) {
    ensureNotInitialized();
    control_blocks_.resize(capacity);
    for (std::size_t i = 0; i < capacity; ++i) {
      freelist_.push_back(static_cast<IndexType>(i));
    }
    initialized_ = true;
  }

  /// @brief Setup empty pool (for lazy/cache pattern - grows on demand)
  /// @param reserveCapacity Optional initial capacity to reserve (default: 0)
  void setupPoolEmpty(std::size_t reserveCapacity = 0) {
    ensureNotInitialized();
    if (reserveCapacity > 0) {
      control_blocks_.reserve(reserveCapacity);
    }
    initialized_ = true;
  }

  /// @brief Expand pool by adding more control blocks
  /// @param additionalCount Number of control blocks to add
  /// @param addToFreelist If true, add new handles to freelist
  /// @return The starting index of new control blocks
  std::size_t expandPool(std::size_t additionalCount,
                         bool addToFreelist = false) {
    ensureInitialized();
    std::size_t oldSize = control_blocks_.size();
    control_blocks_.resize(oldSize + additionalCount);
    if (addToFreelist) {
      for (std::size_t i = oldSize; i < oldSize + additionalCount; ++i) {
        freelist_.push_back(static_cast<IndexType>(i));
      }
    }
    return oldSize;
  }

  /// @brief Teardown pool, calling destroyFn for each control block
  /// @tparam DestroyFn Callable: void(ControlBlock&, Handle)
  /// @note Safe to call when not initialized (no-op)
  template <typename DestroyFn> void teardownPool(DestroyFn &&destroyFn) {
    if (!initialized_) {
      return;
    }
    for (std::size_t i = 0; i < control_blocks_.size(); ++i) {
      // Reconstruct handle for destruction callback using current generation
      Handle h{static_cast<typename Handle::index_type>(i)};
      if constexpr (Handle::has_generation) {
        h.generation = static_cast<typename Handle::generation_type>(
            control_blocks_[i].generation());
      }
      destroyFn(control_blocks_[i], h);
    }
    control_blocks_.clear();
    freelist_.clear();
    initialized_ = false;
  }

  /// @brief Teardown pool without custom destroy logic
  void teardownPool() {
    control_blocks_.clear();
    freelist_.clear();
    initialized_ = false;
  }

  // =========================================================================
  // Freelist Access (LIFO for cache efficiency)
  // =========================================================================

  /// @brief Check if freelist has available slots
  bool hasAvailable() const noexcept { return !freelist_.empty(); }

  /// @brief Pop index from freelist (LIFO) and construct Handle
  /// @return Handle if available, throws if empty
  Handle popFromFreelist() {
    if (freelist_.empty()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::OutOfRange,
          std::string(managerName()) + " freelist is empty");
    }
    IndexType idx = freelist_.back();
    freelist_.pop_back();

    Handle h{idx};
    if constexpr (Handle::has_generation) {
      h.generation = static_cast<typename Handle::generation_type>(
          control_blocks_[idx].generation());
    }
    return h;
  }

  /// @brief Try to pop index from freelist and construct Handle
  /// @return true if successful, false if empty
  bool tryPopFromFreelist(Handle &outHandle) noexcept {
    if (freelist_.empty()) {
      return false;
    }
    IndexType idx = freelist_.back();
    freelist_.pop_back();

    outHandle = Handle{idx};
    if constexpr (Handle::has_generation) {
      outHandle.generation = static_cast<typename Handle::generation_type>(
          control_blocks_[idx].generation());
    }
    return true;
  }

  /// @brief Return handle index to freelist (LIFO)
  /// @note Does not increment generation here; assumes generation was
  /// incremented before calling this or not needed. Used internally by release
  /// methods.
  void pushToFreelist(IndexType idx) noexcept {
    freelist_.push_back(idx); // LIFO: push to back
  }

  /// @brief Allocate a handle from pool, expanding if needed
  /// @param growthSize Number of control blocks to add if pool is empty
  /// @return Handle to an available control block with current generation
  Handle allocate(std::size_t growthSize = 1) {
    ensureInitialized();
    if (freelist_.empty()) {
      expandPool(growthSize, /*addToFreelist=*/true);
    }
    IndexType idx = freelist_.back();
    freelist_.pop_back();

    Handle h{idx};
    if constexpr (Handle::has_generation) {
      h.generation = static_cast<typename Handle::generation_type>(
          control_blocks_[idx].generation());
    }
    return h;
  }

  // =========================================================================
  // Growth Chunk Size Configuration
  // =========================================================================

  /// @brief Get the growth chunk size for pool expansion
  std::size_t growthChunkSize() const noexcept { return growth_chunk_size_; }

  /// @brief Set the growth chunk size for pool expansion
  void setGrowthChunkSize(std::size_t size) {
    if (size == 0) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          std::string(managerName()) + " growth chunk size must be > 0");
    }
    constexpr std::size_t max_index =
        static_cast<std::size_t>(Handle::invalid_index());
    if (size > max_index) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          std::string(managerName()) +
              " growth chunk size exceeds maximum handle range");
    }
    growth_chunk_size_ = size;
  }

  // =========================================================================
  // Acquire Methods
  // =========================================================================

  /// @brief Acquire a fresh resource from pool (allocate + acquire)
  /// @details Allocates a slot from freelist (expanding pool if needed),
  ///          then acquires it using the provided create function.
  /// @param createFn Callable: bool(Payload&) - returns true on success
  /// @return Handle to the acquired resource, or invalid handle on failure
  template <typename CreateFn>
    requires std::invocable<CreateFn, typename ControlBlock::Payload &> &&
             std::convertible_to<
                 std::invoke_result_t<CreateFn,
                                      typename ControlBlock::Payload &>,
                 bool>
  Handle acquireFresh(CreateFn &&createFn) {
    ensureInitialized();
    Handle h = allocate(growth_chunk_size_);
    auto &cb = getControlBlock(h);

    // cb.acquire() handles create internally (idempotent)
    if (!cb.acquire(std::forward<CreateFn>(createFn))) {
      pushToFreelist(h.index);
      return Handle::invalid();
    }

    return h;
  }

  /// @brief Acquire an existing resource by handle (no allocation)
  /// @details Increments the reference count on an existing resource.
  /// @param h Handle to the resource
  /// @param createFn Callable: bool(Payload&) - returns true on success
  /// @return Reference to the control block
  /// @throws OutOfRange if handle is invalid
  /// @throws InvalidState if resource cannot be acquired
  template <typename CreateFn>
    requires std::invocable<CreateFn, typename ControlBlock::Payload &>
  ControlBlock &acquireExisting(Handle h, CreateFn &&createFn) {
    auto &cb = getControlBlockChecked(h);
    if (!cb.acquire(std::forward<CreateFn>(createFn))) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          std::string(managerName()) + " resource cannot be acquired");
    }
    return cb;
  }

  /// @brief Acquire an existing resource by handle (no-op create)
  /// @param h Handle to the resource
  /// @return Reference to the control block
  ControlBlock &acquireExisting(Handle h) {
    return acquireExisting(h, [](auto &) { return true; });
  }

  // =========================================================================
  // Release Helpers
  // =========================================================================

  /// @brief Release for reuse (recycling pattern)
  /// @details Releases the control block and returns slot to freelist if fully
  /// released. Resource remains created for future reuse.
  void releaseForReuse(Handle h) {
    auto idx = static_cast<std::size_t>(h.index);
    if (idx < control_blocks_.size()) {
      auto &cb = control_blocks_[idx];
      if (cb.release()) {
        freelist_.push_back(static_cast<IndexType>(idx));
      }
    }
  }

  /// @brief Release and destroy (non-reusable pattern)
  /// @details Releases the control block, destroys the resource, and returns
  /// slot to freelist if fully released.
  /// @tparam DestroyFn Callable: void(Payload&)
  template <typename DestroyFn>
  void releaseAndDestroy(Handle h, DestroyFn &&destroyFn) {
    auto idx = static_cast<std::size_t>(h.index);
    if (idx < control_blocks_.size()) {
      auto &cb = control_blocks_[idx];
      if (cb.releaseAndDestroy(std::forward<DestroyFn>(destroyFn))) {
        freelist_.push_back(static_cast<IndexType>(idx));
      }
    }
  }

  /// @deprecated Use releaseForReuse() instead
  void releaseToFreelist(Handle h) { releaseForReuse(h); }

  /// @deprecated Use releaseForReuse() instead
  void releaseShared(Handle h) { releaseForReuse(h); }

  /// @deprecated Use releaseForReuse() instead
  void releaseUnique(Handle h) { releaseForReuse(h); }

  // =========================================================================
  // ControlBlock Accessors
  // =========================================================================

  ControlBlock &getControlBlock(Handle h) noexcept {
    return control_blocks_[static_cast<std::size_t>(h.index)];
  }

  const ControlBlock &getControlBlock(Handle h) const noexcept {
    return control_blocks_[static_cast<std::size_t>(h.index)];
  }

  ControlBlock &getControlBlockChecked(Handle h) {
    ensureInitialized();
    auto idx = static_cast<std::size_t>(h.index);
    if (idx >= control_blocks_.size()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::OutOfRange,
          std::string(managerName()) + " invalid handle index");
    }
    // Check generation if Handle supports it
    if constexpr (Handle::has_generation) {
      auto &cb = control_blocks_[idx];
      if (cb.generation() != h.generation) {
        ::orteaf::internal::diagnostics::error::throwError(
            ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
            std::string(managerName()) + " stale handle detected");
      }
    }
    return control_blocks_[idx];
  }

  const ControlBlock &getControlBlockChecked(Handle h) const {
    ensureInitialized();
    auto idx = static_cast<std::size_t>(h.index);
    if (idx >= control_blocks_.size()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::OutOfRange,
          std::string(managerName()) + " invalid handle index");
    }
    return control_blocks_[idx];
  }

  // =========================================================================
  // Capacity & Status
  // =========================================================================

  std::size_t capacity() const noexcept { return control_blocks_.size(); }
  std::size_t available() const noexcept { return freelist_.size(); }
  std::size_t inUse() const noexcept { return capacity() - available(); }
  bool isEmpty() const noexcept { return freelist_.empty(); }
  bool isFull() const noexcept {
    return freelist_.size() == control_blocks_.size();
  }

  bool isValidHandle(Handle h) const noexcept {
    auto idx = static_cast<std::size_t>(h.index);
    if (idx >= control_blocks_.size()) {
      return false;
    }
    if constexpr (Handle::has_generation) {
      if (h.generation != static_cast<typename Handle::generation_type>(
                              control_blocks_[idx].generation())) {
        return false;
      }
    }
    return true;
  }

  // =========================================================================
  // Test Support
  // =========================================================================

#if ORTEAF_ENABLE_TEST
  bool isInitializedForTest() const noexcept { return isInitialized(); }

  std::size_t capacityForTest() const noexcept { return capacity(); }

  std::size_t availableForTest() const noexcept { return available(); }

  std::size_t freeListSizeForTest() const noexcept { return freelist_.size(); }

  const ControlBlock &controlBlockForTest(Handle h) const {
    return control_blocks_[static_cast<std::size_t>(h.index)];
  }

  ControlBlock &controlBlockForTest(Handle h) {
    return control_blocks_[static_cast<std::size_t>(h.index)];
  }

  const ControlBlock &controlBlockForTest(std::size_t index) const {
    return control_blocks_[index];
  }

  ControlBlock &controlBlockForTest(std::size_t index) {
    return control_blocks_[index];
  }
#endif

private:
  bool initialized_{false};
  std::size_t growth_chunk_size_{1};
  std::vector<ControlBlock> control_blocks_;
  std::vector<IndexType> freelist_; // LIFO, stores indices only
};

} // namespace orteaf::internal::runtime::base

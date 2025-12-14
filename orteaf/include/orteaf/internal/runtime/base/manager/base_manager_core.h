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
  /// @tparam CreateFn Callable: void(ControlBlock&, size_t index)
  template <typename CreateFn>
  void setupPool(std::size_t capacity, CreateFn &&createFn) {
    ensureNotInitialized();
    control_blocks_.resize(capacity);
    for (std::size_t i = 0; i < capacity; ++i) {
      createFn(control_blocks_[i], i);
      freelist_.push_back(static_cast<Handle>(i));
    }
    initialized_ = true;
  }

  /// @brief Setup pool with capacity, initializing control blocks to default
  /// @param capacity Number of control blocks to create
  void setupPool(std::size_t capacity) {
    ensureNotInitialized();
    control_blocks_.resize(capacity);
    for (std::size_t i = 0; i < capacity; ++i) {
      freelist_.push_back(static_cast<Handle>(i));
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
        freelist_.push_back(static_cast<Handle>(i));
      }
    }
    return oldSize;
  }

  /// @brief Teardown pool, calling destroyFn for each control block
  /// @tparam DestroyFn Callable: void(ControlBlock&, Handle)
  template <typename DestroyFn> void teardownPool(DestroyFn &&destroyFn) {
    for (std::size_t i = 0; i < control_blocks_.size(); ++i) {
      destroyFn(control_blocks_[i], static_cast<Handle>(i));
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

  /// @brief Pop index from freelist (LIFO)
  /// @return Handle if available, throws if empty
  Handle popFromFreelist() {
    if (freelist_.empty()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::OutOfRange,
          std::string(managerName()) + " freelist is empty");
    }
    Handle h = freelist_.back(); // LIFO: pop from back
    freelist_.pop_back();
    return h;
  }

  /// @brief Try to pop index from freelist
  /// @return true if successful, false if empty
  bool tryPopFromFreelist(Handle &outHandle) noexcept {
    if (freelist_.empty()) {
      return false;
    }
    outHandle = freelist_.back();
    freelist_.pop_back();
    return true;
  }

  /// @brief Return handle to freelist (LIFO)
  void pushToFreelist(Handle h) noexcept {
    freelist_.push_back(h); // LIFO: push to back
  }

  /// @brief Allocate a handle from pool, expanding if needed
  /// @param growthSize Number of control blocks to add if pool is empty
  /// @return Handle to an available control block
  Handle allocate(std::size_t growthSize = 1) {
    ensureInitialized();
    if (freelist_.empty()) {
      expandPool(growthSize, /*addToFreelist=*/true);
    }
    Handle h = freelist_.back();
    freelist_.pop_back();
    return h;
  }

  // =========================================================================
  // Acquire Helpers
  // =========================================================================

  /// @brief Acquire from pool, throws if empty (fixed-size pool)
  /// @return Handle to acquired resource
  Handle acquireFromPool() {
    ensureInitialized();
    if (freelist_.empty()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::OutOfRange,
          std::string(managerName()) + " pool is exhausted");
    }
    Handle h = freelist_.back();
    freelist_.pop_back();
    control_blocks_[static_cast<std::size_t>(h.index)].tryAcquire();
    return h;
  }

  /// @brief Acquire, creating resource if slot not initialized (growable pool)
  /// @param growthSize How much to grow if pool is empty
  /// @param createFn Callable: bool(ControlBlock&, Handle) - returns true on
  /// success
  /// @return Handle to the acquired resource, or invalid handle on creation
  /// failure
  /// @note Only calls createFn when slot.isInitialized() returns false
  /// @note On createFn failure, handle is returned to freelist
  template <typename CreateFn>
    requires std::invocable<CreateFn, ControlBlock &, Handle> &&
             std::convertible_to<
                 std::invoke_result_t<CreateFn, ControlBlock &, Handle>, bool>
  Handle acquireOrCreate(std::size_t growthSize, CreateFn &&createFn) {
    ensureInitialized();
    Handle h = allocate(growthSize); // Auto-grows if empty
    auto &cb = getControlBlock(h);

    // Create only if not initialized
    if (!cb.slot.isInitialized()) {
      if (!createFn(cb, h)) {
        pushToFreelist(h);
        return Handle::invalid(); // Invalid handle on failure
      }
      cb.slot.markInitialized();
    }

    cb.tryAcquire();
    return h;
  }

  // =========================================================================
  // Release Helpers
  // =========================================================================

  /// @brief Release to freelist (reusable)
  void releaseToFreelist(Handle h) {
    auto idx = static_cast<std::size_t>(h.index);
    if (idx < control_blocks_.size()) {
      control_blocks_[idx].release();
      freelist_.push_back(h); // LIFO
    }
  }

  /// @brief Release a shared reference. If ref count drops to zero, return to
  /// freelist.
  /// @note Requires ControlBlock::release() to return bool (true if count
  /// became 0)
  void releaseShared(Handle h) {
    auto idx = static_cast<std::size_t>(h.index);
    if (idx < control_blocks_.size()) {
      auto &cb = control_blocks_[idx];
      if (cb.release()) {
        freelist_.push_back(h); // LIFO
      }
    }
  }

  /// @brief Release and destroy (non-reusable)
  /// @tparam DestroyFn Callable: void(ControlBlock&, Handle)
  template <typename DestroyFn>
  void releaseAndDestroy(Handle h, DestroyFn &&destroyFn) {
    auto idx = static_cast<std::size_t>(h.index);
    if (idx < control_blocks_.size()) {
      auto &cb = control_blocks_[idx];
      cb.release();
      destroyFn(cb, h);

      // Mark as uninitialized for Slot<T>
      if constexpr (requires { cb.slot.markUninitialized(); }) {
        cb.slot.markUninitialized();
      }

      freelist_.push_back(h); // LIFO
    }
  }

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
    return static_cast<std::size_t>(h.index) < control_blocks_.size();
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
  std::vector<ControlBlock> control_blocks_;
  std::vector<Handle> freelist_; // LIFO for cache efficiency
};

} // namespace orteaf::internal::runtime::base

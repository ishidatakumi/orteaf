#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <orteaf/internal/diagnostics/error/error.h>
#include <orteaf/internal/runtime/base/lease/concepts.h>

namespace orteaf::internal::runtime::base {

/// @brief Base manager providing common infrastructure for resource management
/// @tparam ControlBlockT The control block type (e.g.,
/// SharedControlBlock<Slot<T>>)
/// @tparam HandleT The handle type used to identify resources
template <typename ControlBlockT, typename HandleT>
  requires ControlBlockConcept<ControlBlockT>
class BaseManagerCore {
protected:
  using ControlBlock = ControlBlockT;
  using Handle = HandleT;

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
          "Manager has not been initialized");
    }
  }

  /// @brief Ensure manager is not initialized, throw if already initialized
  void ensureNotInitialized() const {
    if (initialized_) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "Manager already initialized");
    }
  }

  // =========================================================================
  // Initialize
  // =========================================================================

  /// @brief Initialize slots with capacity, calling createFn for each
  /// @tparam CreateFn Callable: void(ControlBlock&, size_t index)
  template <typename CreateFn>
  void initializeSlots(std::size_t capacity, CreateFn &&createFn) {
    ensureNotInitialized();
    control_blocks_.resize(capacity);
    for (std::size_t i = 0; i < capacity; ++i) {
      createFn(control_blocks_[i], i);
      freelist_.push_back(static_cast<Handle>(i));
    }
    initialized_ = true;
  }

  /// @brief Reserve capacity without initializing slots
  void reserveSlots(std::size_t capacity) {
    ensureNotInitialized();
    control_blocks_.resize(capacity);
    for (std::size_t i = 0; i < capacity; ++i) {
      freelist_.push_back(static_cast<Handle>(i));
    }
    initialized_ = true;
  }

  /// @brief Expand capacity by adding more slots (does not add to freelist)
  /// @return The starting index of new slots
  std::size_t expandCapacity(std::size_t additionalCount) {
    ensureInitialized();
    std::size_t oldSize = control_blocks_.size();
    control_blocks_.resize(oldSize + additionalCount);
    return oldSize;
  }

  /// @brief Add a single slot to freelist
  void addToFreelist(Handle h) {
    freelist_.push_back(h); // LIFO: push to back
  }

  // =========================================================================
  // Shutdown
  // =========================================================================

  /// @brief Shutdown all slots, calling destroyFn for each slot
  /// @tparam DestroyFn Callable: void(ControlBlock&, Handle)
  template <typename DestroyFn> void shutdownSlots(DestroyFn &&destroyFn) {
    for (std::size_t i = 0; i < control_blocks_.size(); ++i) {
      destroyFn(control_blocks_[i], static_cast<Handle>(i));
    }
    control_blocks_.clear();
    freelist_.clear();
    initialized_ = false;
  }

  /// @brief Shutdown all slots without custom destroy logic
  void shutdownSlots() {
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
          "Freelist is empty");
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

  // =========================================================================
  // Acquire Helpers
  // =========================================================================

  /// @brief Acquire from fixed set (immutable manager)
  /// @return Handle if available, or invalid handle
  Handle acquireFromFixed() {
    ensureInitialized();
    if (freelist_.empty()) {
      return Handle{}; // Invalid
    }
    Handle h = freelist_.back(); // LIFO
    freelist_.pop_back();
    control_blocks_[static_cast<std::size_t>(h)].tryAcquire();
    return h;
  }

  /// @brief Acquire, creating if needed
  /// @tparam CreateFn Callable: void(ControlBlock&, Handle) - called if slot
  /// not initialized
  template <typename CreateFn> Handle acquireOrCreate(CreateFn &&createFn) {
    ensureInitialized();
    if (freelist_.empty()) {
      return Handle{}; // No space
    }
    Handle h = freelist_.back(); // LIFO
    freelist_.pop_back();
    auto &cb = control_blocks_[static_cast<std::size_t>(h)];

    // Create if not initialized (for Slot<T> with initialized flag)
    if constexpr (requires { cb.payload.isInitialized(); }) {
      if (!cb.payload.isInitialized()) {
        createFn(cb, h);
        cb.payload.markInitialized();
      }
    }

    cb.tryAcquire();
    return h;
  }

  // =========================================================================
  // Release Helpers
  // =========================================================================

  /// @brief Release to freelist (reusable)
  void releaseToFreelist(Handle h) {
    auto idx = static_cast<std::size_t>(h);
    if (idx < control_blocks_.size()) {
      control_blocks_[idx].release();
      freelist_.push_back(h); // LIFO
    }
  }

  /// @brief Release and destroy (non-reusable)
  /// @tparam DestroyFn Callable: void(ControlBlock&, Handle)
  template <typename DestroyFn>
  void releaseAndDestroy(Handle h, DestroyFn &&destroyFn) {
    auto idx = static_cast<std::size_t>(h);
    if (idx < control_blocks_.size()) {
      auto &cb = control_blocks_[idx];
      cb.release();
      destroyFn(cb, h);

      // Mark as uninitialized for Slot<T>
      if constexpr (requires { cb.payload.markUninitialized(); }) {
        cb.payload.markUninitialized();
      }

      freelist_.push_back(h); // LIFO
    }
  }

  // =========================================================================
  // ControlBlock Accessors
  // =========================================================================

  ControlBlock &getControlBlock(Handle h) noexcept {
    return control_blocks_[static_cast<std::size_t>(h)];
  }

  const ControlBlock &getControlBlock(Handle h) const noexcept {
    return control_blocks_[static_cast<std::size_t>(h)];
  }

  ControlBlock &getControlBlockChecked(Handle h) {
    ensureInitialized();
    auto idx = static_cast<std::size_t>(h);
    if (idx >= control_blocks_.size()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::OutOfRange,
          "Invalid handle index");
    }
    return control_blocks_[idx];
  }

  const ControlBlock &getControlBlockChecked(Handle h) const {
    ensureInitialized();
    auto idx = static_cast<std::size_t>(h);
    if (idx >= control_blocks_.size()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::OutOfRange,
          "Invalid handle index");
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
    return static_cast<std::size_t>(h) < control_blocks_.size();
  }

private:
  bool initialized_{false};
  std::vector<ControlBlock> control_blocks_;
  std::vector<Handle> freelist_; // LIFO for cache efficiency
};

} // namespace orteaf::internal::runtime::base

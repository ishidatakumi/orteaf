#pragma once

#include <atomic>
#include <cstdint>

#include <orteaf/internal/runtime/base/lease/category.h>
#include <orteaf/internal/runtime/base/lease/concepts.h>
#include <orteaf/internal/runtime/base/lease/slot.h>

namespace orteaf::internal::runtime::base {

/// @brief Shared control block - shared ownership with reference counting
/// @details Multiple leases can share this resource. Uses atomic reference
/// count for thread-safe sharing.
template <typename SlotT>
  requires SlotConcept<SlotT>
class SharedControlBlock {
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

  // =========================================================================
  // Concept Required API
  // =========================================================================

  /// @brief Acquire a shared reference (increment count)
  /// @return always true for shared resources
  bool acquire() noexcept {
    strong_count_.fetch_add(1, std::memory_order_relaxed);
    return true;
  }

  /// @brief Release a shared reference
  /// @return true if this was the last reference (count goes 1->0)
  /// @note Automatically increments generation if this was the last reference
  bool release() noexcept {
    if (strong_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      if constexpr (SlotT::has_generation) {
        slot_.incrementGeneration();
      }
      return true;
    }
    return false;
  }

  /// @brief Check if any references exist
  bool isAlive() const noexcept { return count() > 0; }

  /// @brief Mark slot as initialized/valid
  void validate() noexcept {
    if constexpr (SlotT::has_initialized) {
      slot_.markInitialized();
    }
  }

  /// @brief Mark slot as uninitialized/invalid
  void invalidate() noexcept {
    if constexpr (SlotT::has_initialized) {
      slot_.markUninitialized();
    }
  }

  // =========================================================================
  // Shared-specific API (SharedControlBlockConcept)
  // =========================================================================

  /// @brief Get current reference count
  std::uint32_t count() const noexcept {
    return strong_count_.load(std::memory_order_acquire);
  }

  // =========================================================================
  // Payload Access
  // =========================================================================

  /// @brief Access the payload
  Payload &payload() noexcept { return slot_.get(); }
  const Payload &payload() const noexcept { return slot_.get(); }

  // =========================================================================
  // Additional Queries
  // =========================================================================

  /// @brief Check if slot is initialized
  bool isInitialized() const noexcept { return slot_.isInitialized(); }

  /// @brief Get current generation (0 if not supported)
  auto generation() const noexcept { return slot_.generation(); }

private:
  std::atomic<std::uint32_t> strong_count_{0};
  SlotT slot_{};
};

// Verify concept satisfaction
static_assert(ControlBlockConcept<SharedControlBlock<RawSlot<int>>>);
static_assert(SharedControlBlockConcept<SharedControlBlock<RawSlot<int>>>);

} // namespace orteaf::internal::runtime::base

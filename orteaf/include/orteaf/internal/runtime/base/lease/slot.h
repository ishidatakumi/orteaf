#pragma once

#include <cstdint>
#include <type_traits>

namespace orteaf::internal::runtime::base {

// =============================================================================
// 1. RawSlot - No initialized flag, no generation
// =============================================================================

/// @brief Raw slot without any tracking
/// @details Simply wraps a payload. Used for immutable resources.
template <typename PayloadT> struct RawSlot {
  using Payload = PayloadT;

  PayloadT payload{};

  static constexpr bool has_generation = false;
  static constexpr bool isInitialized() noexcept { return true; }
  static constexpr void markInitialized() noexcept {}
  static constexpr void markUninitialized() noexcept {}
  static constexpr std::uint32_t generation() noexcept { return 0; }
  static constexpr void incrementGeneration() noexcept {}

  constexpr PayloadT &get() noexcept { return payload; }
  constexpr const PayloadT &get() const noexcept { return payload; }
  constexpr PayloadT *operator->() noexcept { return &payload; }
  constexpr const PayloadT *operator->() const noexcept { return &payload; }
  constexpr PayloadT &operator*() noexcept { return payload; }
  constexpr const PayloadT &operator*() const noexcept { return payload; }
};

// =============================================================================
// 2. Slot - With initialized flag, no generation
// =============================================================================

/// @brief Slot with initialization tracking
/// @details Wraps a payload with an `initialized` boolean flag.
template <typename PayloadT> struct Slot {
  using Payload = PayloadT;

  bool initialized_{false};
  PayloadT payload{};

  static constexpr bool has_generation = false;
  constexpr bool isInitialized() const noexcept { return initialized_; }
  constexpr void markInitialized() noexcept { initialized_ = true; }
  constexpr void markUninitialized() noexcept { initialized_ = false; }
  static constexpr std::uint32_t generation() noexcept { return 0; }
  static constexpr void incrementGeneration() noexcept {}

  constexpr PayloadT &get() noexcept { return payload; }
  constexpr const PayloadT &get() const noexcept { return payload; }
  constexpr PayloadT *operator->() noexcept { return &payload; }
  constexpr const PayloadT *operator->() const noexcept { return &payload; }
  constexpr PayloadT &operator*() noexcept { return payload; }
  constexpr const PayloadT &operator*() const noexcept { return payload; }
};

// =============================================================================
// 3. GenerationalRawSlot - No initialized flag, with generation
// =============================================================================

/// @brief Raw slot with generation tracking
/// @details Wraps a payload with a generation counter for ABA problem
/// prevention.
/// @tparam PayloadT The payload type
/// @tparam GenerationT The generation counter type (default: uint32_t)
template <typename PayloadT, typename GenerationT = std::uint32_t>
struct GenerationalRawSlot {
  using Payload = PayloadT;
  using Generation = GenerationT;

  GenerationT generation_{0};
  PayloadT payload{};

  static constexpr bool has_generation = true;
  static constexpr bool isInitialized() noexcept { return true; }
  static constexpr void markInitialized() noexcept {}
  static constexpr void markUninitialized() noexcept {}
  constexpr GenerationT generation() const noexcept { return generation_; }
  constexpr void incrementGeneration() noexcept { ++generation_; }

  constexpr PayloadT &get() noexcept { return payload; }
  constexpr const PayloadT &get() const noexcept { return payload; }
  constexpr PayloadT *operator->() noexcept { return &payload; }
  constexpr const PayloadT *operator->() const noexcept { return &payload; }
  constexpr PayloadT &operator*() noexcept { return payload; }
  constexpr const PayloadT &operator*() const noexcept { return payload; }
};

// =============================================================================
// 4. GenerationalSlot - With initialized flag and generation
// =============================================================================

/// @brief Slot with initialization tracking and generation counter
/// @details Full-featured slot with both initialization flag and generation.
/// @tparam PayloadT The payload type
/// @tparam GenerationT The generation counter type (default: uint32_t)
template <typename PayloadT, typename GenerationT = std::uint32_t>
struct GenerationalSlot {
  using Payload = PayloadT;
  using Generation = GenerationT;

  bool initialized_{false};
  GenerationT generation_{0};
  PayloadT payload{};

  static constexpr bool has_generation = true;
  constexpr bool isInitialized() const noexcept { return initialized_; }
  constexpr void markInitialized() noexcept { initialized_ = true; }
  constexpr void markUninitialized() noexcept { initialized_ = false; }
  constexpr GenerationT generation() const noexcept { return generation_; }
  constexpr void incrementGeneration() noexcept { ++generation_; }

  constexpr PayloadT &get() noexcept { return payload; }
  constexpr const PayloadT &get() const noexcept { return payload; }
  constexpr PayloadT *operator->() noexcept { return &payload; }
  constexpr const PayloadT *operator->() const noexcept { return &payload; }
  constexpr PayloadT &operator*() noexcept { return payload; }
  constexpr const PayloadT &operator*() const noexcept { return payload; }
};

// =============================================================================
// Concepts
// =============================================================================

template <typename S>
concept SlotWrapperConcept = requires(S s, const S cs) {
  typename S::Payload;
  { cs.isInitialized() } -> std::same_as<bool>;
  { s.markInitialized() };
  { s.markUninitialized() };
  { cs.generation() } -> std::convertible_to<std::uint32_t>;
  { s.incrementGeneration() };
  { s.get() };
};

template <typename S>
concept GenerationalSlotWrapperConcept =
    SlotWrapperConcept<S> && requires(const S cs) {
      { cs.generation() } -> std::same_as<std::uint32_t>;
    };

} // namespace orteaf::internal::runtime::base

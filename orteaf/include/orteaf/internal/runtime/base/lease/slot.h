#pragma once

#include <cstdint>
#include <type_traits>

namespace orteaf::internal::runtime::base {

// =============================================================================
// 1. RawSlot - Payload only, no generation
// =============================================================================

/// @brief Raw slot without generation tracking
/// @details Simply wraps a payload. Initialization is managed by ControlBlock.
template <typename PayloadT> struct RawSlot {
  using Payload = PayloadT;

  PayloadT payload{};

  static constexpr bool has_generation = false;
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
// 2. GenerationalSlot - Payload with generation counter
// =============================================================================

/// @brief Slot with generation tracking for ABA problem prevention
/// @details Wraps a payload with a generation counter. Initialization is
/// managed by ControlBlock.
/// @tparam PayloadT The payload type
/// @tparam GenerationT The generation counter type (default: uint32_t)
template <typename PayloadT, typename GenerationT = std::uint32_t>
struct GenerationalSlot {
  using Payload = PayloadT;
  using Generation = GenerationT;

  GenerationT generation_{0};
  PayloadT payload{};

  static constexpr bool has_generation = true;
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
concept SlotConcept = requires(S s, const S cs) {
  typename S::Payload;
  { S::has_generation } -> std::convertible_to<bool>;
  { cs.generation() } -> std::convertible_to<std::uint32_t>;
  { s.incrementGeneration() };
  { s.get() };
};

template <typename S>
concept GenerationalSlotConcept =
    SlotConcept<S> && S::has_generation && requires(const S cs) {
      { cs.generation() } -> std::same_as<typename S::Generation>;
    };

} // namespace orteaf::internal::runtime::base

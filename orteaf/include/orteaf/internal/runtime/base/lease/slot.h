#pragma once

#include <concepts>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace orteaf::internal::runtime::base {

// =============================================================================
// 1. RawSlot - Payload only, no generation
// =============================================================================

/// @brief Raw slot without generation tracking
/// @details Wraps a payload with creation state tracking. Creation state can
/// only be changed by executing actual create/destroy operations via lambdas.
template <typename PayloadT> class RawSlot {
public:
  using Payload = PayloadT;

  static constexpr bool has_generation = false;
  static constexpr std::uint32_t generation() noexcept { return 0; }
  static constexpr void incrementGeneration() noexcept {}

  // Payload access
  constexpr PayloadT &get() noexcept { return payload_; }
  constexpr const PayloadT &get() const noexcept { return payload_; }
  constexpr PayloadT *operator->() noexcept { return &payload_; }
  constexpr const PayloadT *operator->() const noexcept { return &payload_; }
  constexpr PayloadT &operator*() noexcept { return payload_; }
  constexpr const PayloadT &operator*() const noexcept { return payload_; }

  // Creation state query
  constexpr bool isCreated() const noexcept { return is_created_; }

  /// @brief Create the resource by executing the factory (idempotent)
  /// @tparam Factory Callable that takes Payload& and returns bool
  /// @return true if created or already created, false if factory failed
  template <typename Factory>
    requires std::invocable<Factory, PayloadT &> &&
             std::convertible_to<std::invoke_result_t<Factory, PayloadT &>,
                                 bool>
  bool create(Factory &&factory) {
    if (is_created_) {
      return true; // Already created
    }
    bool success = std::forward<Factory>(factory)(payload_);
    if (success) {
      is_created_ = true;
    }
    return success;
  }

  /// @brief Destroy the resource by executing the destructor (idempotent)
  /// @tparam Destructor Callable that takes Payload& and cleans it up
  /// @return true if destroyed, false if not created
  template <typename Destructor>
    requires std::invocable<Destructor, PayloadT &>
  bool destroy(Destructor &&destructor) {
    if (!is_created_) {
      return false; // Not created, nothing to destroy
    }
    std::forward<Destructor>(destructor)(payload_);
    is_created_ = false;
    return true;
  }

private:
  bool is_created_{false};
  PayloadT payload_{};
};

// =============================================================================
// 2. GenerationalSlot - Payload with generation counter
// =============================================================================

/// @brief Slot with generation tracking for ABA problem prevention
/// @details Wraps a payload with a generation counter and creation state.
/// Creation state can only be changed via create/destroy operations.
/// @tparam PayloadT The payload type
/// @tparam GenerationT The generation counter type (default: uint32_t)
template <typename PayloadT, typename GenerationT = std::uint32_t>
class GenerationalSlot {
public:
  using Payload = PayloadT;
  using Generation = GenerationT;

  static constexpr bool has_generation = true;
  constexpr GenerationT generation() const noexcept { return generation_; }
  constexpr void incrementGeneration() noexcept { ++generation_; }

  // Payload access
  constexpr PayloadT &get() noexcept { return payload_; }
  constexpr const PayloadT &get() const noexcept { return payload_; }
  constexpr PayloadT *operator->() noexcept { return &payload_; }
  constexpr const PayloadT *operator->() const noexcept { return &payload_; }
  constexpr PayloadT &operator*() noexcept { return payload_; }
  constexpr const PayloadT &operator*() const noexcept { return payload_; }

  // Creation state query
  constexpr bool isCreated() const noexcept { return is_created_; }

  /// @brief Create the resource by executing the factory (idempotent)
  /// @tparam Factory Callable that takes Payload& and returns bool
  /// @return true if created or already created, false if factory failed
  template <typename Factory>
    requires std::invocable<Factory, PayloadT &> &&
             std::convertible_to<std::invoke_result_t<Factory, PayloadT &>,
                                 bool>
  bool create(Factory &&factory) {
    if (is_created_) {
      return true; // Already created
    }
    bool success = std::forward<Factory>(factory)(payload_);
    if (success) {
      is_created_ = true;
    }
    return success;
  }

  /// @brief Destroy the resource by executing the destructor (idempotent)
  /// @tparam Destructor Callable that takes Payload& and cleans it up
  /// @return true if destroyed, false if not created
  template <typename Destructor>
    requires std::invocable<Destructor, PayloadT &>
  bool destroy(Destructor &&destructor) {
    if (!is_created_) {
      return false; // Not created, nothing to destroy
    }
    std::forward<Destructor>(destructor)(payload_);
    is_created_ = false;
    return true;
  }

private:
  bool is_created_{false};
  GenerationT generation_{0};
  PayloadT payload_{};
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
  // Creation state
  { cs.isCreated() } -> std::same_as<bool>;
};

template <typename S>
concept GenerationalSlotConcept =
    SlotConcept<S> && S::has_generation && requires(const S cs) {
      { cs.generation() } -> std::same_as<typename S::Generation>;
    };

} // namespace orteaf::internal::runtime::base

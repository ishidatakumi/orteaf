#pragma once

#include <cstdint>
#include <compare>
#include <type_traits>
#include <limits>

namespace orteaf::internal::base {

/**
 * @brief 世代検証付きの軽量ハンドル（世代なしも選択可）。
 *
 * Generation を void にすると世代フィールドを持たない軽量版になる。
 * 無効値は index の最大値で判定する。
 */
template <class Tag, class Index = uint32_t, class Generation = uint8_t>
struct Handle {
  static_assert(std::is_unsigned_v<Index>, "Handle index must be unsigned");
    static_assert(std::is_unsigned_v<Generation>, "Handle generation must be unsigned");

  using tag_type = Tag;
  using index_type = Index;
  using generation_type = Generation;
  using underlying_type = Index;
  static constexpr bool has_generation = true;

  index_type index{};
  generation_type generation{};

  constexpr Handle() = default;
    constexpr Handle(index_type idx, generation_type gen = generation_type{}) noexcept
      : index(idx), generation(gen) {}

    constexpr auto operator<=>(const Handle&) const = default;
  explicit constexpr operator underlying_type() const noexcept { return index; }

    static constexpr Handle invalid() noexcept { return Handle{invalid_index(), invalid_generation()}; }
  constexpr bool isValid() const noexcept { return index != invalid_index(); }

    static constexpr index_type invalid_index() noexcept { return std::numeric_limits<index_type>::max(); }
  static constexpr generation_type invalid_generation() noexcept {
    return std::numeric_limits<generation_type>::max();
  }
};

// 世代フィールドを持たない版（Generation = void）
template <class Tag, class Index>
struct Handle<Tag, Index, void> {
  static_assert(std::is_unsigned_v<Index>, "Handle index must be unsigned");

  using tag_type = Tag;
  using index_type = Index;
  using generation_type = void;
  using underlying_type = Index;
  static constexpr bool has_generation = false;

  index_type index{};

  constexpr Handle() = default;
  constexpr explicit Handle(index_type idx) noexcept : index(idx) {}

    constexpr auto operator<=>(const Handle&) const = default;
  explicit constexpr operator underlying_type() const noexcept { return index; }

  static constexpr Handle invalid() noexcept { return Handle{invalid_index()}; }
  constexpr bool isValid() const noexcept { return index != invalid_index(); }

    static constexpr index_type invalid_index() noexcept { return std::numeric_limits<index_type>::max(); }
};

struct DeviceTag {};
struct StreamTag {};
struct ContextTag {};
struct CommandQueueTag {};
struct LibraryTag {};
struct FunctionTag {};
struct HeapTag {};
struct BufferTag {};
struct GraphTag {};
struct EventTag {};
struct FenceTag {};

using DeviceHandle       = Handle<DeviceTag, uint32_t, void>;
using StreamHandle       = Handle<StreamTag, uint32_t, uint8_t>;
using ContextHandle      = Handle<ContextTag, uint32_t, uint8_t>;
using CommandQueueHandle = Handle<CommandQueueTag, uint32_t, void>;
using LibraryHandle      = Handle<LibraryTag, uint32_t, void>;
using FunctionHandle     = Handle<FunctionTag, uint32_t, void>;
using HeapHandle         = Handle<HeapTag, uint32_t, void>;
using BufferHandle       = Handle<BufferTag, uint32_t, uint32_t>;
using BufferViewHandle   = Handle<BufferTag, uint32_t, void>;
using GraphHandle        = Handle<GraphTag, uint32_t, uint8_t>;
using EventHandle        = Handle<EventTag, uint32_t, uint8_t>;
using FenceHandle        = Handle<FenceTag, uint32_t, uint8_t>;

static_assert(std::is_trivially_copyable_v<DeviceHandle>);
static_assert(std::is_trivially_copyable_v<BufferHandle>);
static_assert(std::is_trivially_copyable_v<BufferViewHandle>);
static_assert(std::is_trivially_copyable_v<EventHandle>);
static_assert(std::is_trivially_copyable_v<FenceHandle>);

} // namespace orteaf::internal::base

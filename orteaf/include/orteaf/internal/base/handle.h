#pragma once

#include <cstdint>
#include <compare>
#include <type_traits>
#include <limits>

namespace orteaf::internal::base {

/**
 * @brief 世代検証付きの軽量ハンドル。
 *
 * index/generation の型をテンプレートで渡せるようにし、用途ごとにビット幅を選べる。
 * 無効値は index の最大値で判定し、generation も同じく最大値をセットする。
 */
template <class Tag, class Index = uint32_t, class Generation = uint8_t>
struct Handle {
    static_assert(std::is_unsigned_v<Index>, "Handle index must be unsigned");
    static_assert(std::is_unsigned_v<Generation>, "Handle generation must be unsigned");

    using tag_type = Tag;
    using index_type = Index;
    using generation_type = Generation;
    using underlying_type = Index;

    index_type index{};
    generation_type generation{};

    constexpr Handle() = default;
    constexpr Handle(index_type idx, generation_type gen = generation_type{}) noexcept
        : index(idx), generation(gen) {}

    constexpr auto operator<=>(const Handle&) const = default;
    explicit constexpr operator underlying_type() const noexcept { return index; }

    static constexpr Handle invalid() noexcept {
        return Handle{invalid_index(), invalid_generation()};
    }

    constexpr bool isValid() const noexcept { return index != invalid_index(); }

    static constexpr index_type invalid_index() noexcept {
        return std::numeric_limits<index_type>::max();
    }

    static constexpr generation_type invalid_generation() noexcept {
        return std::numeric_limits<generation_type>::max();
    }
};

struct DeviceTag {};
struct StreamTag {};
struct ContextTag {};
struct CommandQueueTag {};
struct LibraryTag {};
struct FunctionTag {};
struct HeapTag {};
struct BufferTag {};

using DeviceId  = Handle<DeviceTag, uint32_t, uint8_t>;
using StreamId  = Handle<StreamTag, uint32_t, uint8_t>;
using ContextId = Handle<ContextTag, uint32_t, uint8_t>;
using CommandQueueId = Handle<CommandQueueTag, uint32_t, uint8_t>;
using LibraryId = Handle<LibraryTag, uint32_t, uint8_t>;
using FunctionId = Handle<FunctionTag, uint32_t, uint8_t>;
using HeapId = Handle<HeapTag, uint32_t, uint8_t>;
using BufferHandle = Handle<BufferTag, uint32_t, uint16_t>;
using BufferId = BufferHandle;

static_assert(sizeof(DeviceId) == sizeof(uint32_t) + sizeof(uint8_t) + 3); // Padding likely
static_assert(std::is_trivially_copyable_v<DeviceId>);
static_assert(std::is_trivially_copyable_v<BufferHandle>);

} // namespace orteaf::internal::base

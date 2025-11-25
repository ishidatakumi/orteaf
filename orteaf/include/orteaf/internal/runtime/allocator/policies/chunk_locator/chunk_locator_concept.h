#pragma once

/**
 * @file chunk_locator_concept.h
 * @brief ChunkLocator ポリシーの concept 定義。
 */

#include <concepts>
#include <cstddef>
#include <type_traits>

#include "orteaf/internal/base/strong_id.h"

namespace orteaf::internal::runtime::allocator::policies {

/**
 * @brief ChunkLocator ポリシーが満たすべき concept。
 *
 * ChunkLocator は、チャンク（メモリブロック）の確保・解放・参照カウント管理を行う。
 * Direct、Hierarchical などの異なる実装がこの concept を満たす。
 *
 * @tparam T ChunkLocator 実装型
 * @tparam Config 設定型（ChunkLocatorConfigBase を継承）
 * @tparam Resource リソース管理型
 */
template <typename T, typename Config, typename Resource>
concept ChunkLocator = requires(
    T& locator,
    const T& const_locator,
    const Config& config,
    Resource* resource,
    std::size_t size,
    std::size_t alignment,
    typename T::BufferId id
) {
    // 型エイリアスの存在確認
    typename T::BufferId;
    typename T::BufferView;
    typename T::MemoryBlock;
    typename T::Device;
    typename T::Context;
    typename T::Stream;
    typename T::Config;

    // 初期化
    { locator.initialize(config, resource) } -> std::same_as<void>;

    // チャンク確保・解放
    { locator.addChunk(size, alignment) } -> std::same_as<typename T::MemoryBlock>;
    { locator.releaseChunk(id) } -> std::same_as<bool>;

    // チャンク情報取得
    { const_locator.findChunkSize(id) } -> std::same_as<std::size_t>;
    { const_locator.isAlive(id) } -> std::same_as<bool>;

    // 参照カウント操作
    { locator.incrementUsed(id) } -> std::same_as<void>;
    { locator.decrementUsed(id) } -> std::same_as<void>;
    { locator.incrementPending(id) } -> std::same_as<void>;
    { locator.decrementPending(id) } -> std::same_as<void>;
    { locator.decrementPendingAndUsed(id) } -> std::same_as<void>;
};

/**
 * @brief BufferId が共通の型であることを確認する concept。
 */
template <typename T>
concept HasStandardBufferId = std::same_as<typename T::BufferId, ::orteaf::internal::base::BufferId>;

/**
 * @brief ChunkLocator の Config が基底クラスを継承していることを確認するヘルパー。
 *
 * 使用例:
 * @code
 * static_assert(ChunkLocatorConfigDerived<Policy::Config, Device, Context, Stream>);
 * @endcode
 */
template <typename Config, typename Device, typename Context, typename Stream>
concept ChunkLocatorConfigDerived = requires(const Config& cfg) {
    { cfg.device } -> std::convertible_to<Device>;
    { cfg.context } -> std::convertible_to<Context>;
    { cfg.stream } -> std::convertible_to<Stream>;
};

}  // namespace orteaf::internal::runtime::allocator::policies

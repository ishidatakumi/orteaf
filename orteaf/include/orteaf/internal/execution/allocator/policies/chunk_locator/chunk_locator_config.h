#pragma once

/**
 * @file chunk_locator_config.h
 * @brief ChunkLocator ポリシー用の共通 Config 基底クラス。
 */

namespace orteaf::internal::execution::allocator::policies {

/**
 * @brief ChunkLocator 共通の設定基底クラス。
 *
 * 各 ChunkLocator ポリシーはこのクラスを継承して固有の設定を追加する。
 * 仮想関数を持たないため、継承によるオーバーヘッドはない。
 *
 * @tparam Device デバイス型
 * @tparam Context コンテキスト型
 * @tparam Stream ストリーム型
 */
template <typename Device, typename Context, typename Stream>
struct ChunkLocatorConfigBase {
    Device device{};
    Context context{};
    Stream stream{};
};

}  // namespace orteaf::internal::execution::allocator::policies

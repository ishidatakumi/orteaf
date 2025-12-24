#pragma once

#include <cstddef>

namespace orteaf::internal::execution::allocator::policies {

/**
 * @brief ポリシー共通の Config 基底。
 *
 * リソースは非所有ポインタとして受け取り、実装側で有効性を検証する。
 * Resource が static API の場合も、初期化忘れ検出用にダミーインスタンスを渡す。
 */
template <typename Resource>
struct PolicyConfig {
    Resource* resource{nullptr};
};

}  // namespace orteaf::internal::execution::allocator::policies

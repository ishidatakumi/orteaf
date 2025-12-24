#pragma once

#include <bit>
#include <cstddef>

namespace orteaf::internal::execution::allocator {

/**
 * @brief サイズクラス計算ユーティリティ
 *
 * SegregatePool やフリーリストポリシーで使用するサイズクラス計算を一元化。
 * サイズクラスは2の冪乗で分類し、min_block_size
 * を基準としたインデックスを返す。
 */

/**
 * @brief ブロックサイズからサイズクラスインデックスを計算
 *
 * @param block_size 対象のブロックサイズ
 * @param min_block_size 最小ブロックサイズ（サイズクラス0に対応）
 * @return サイズクラスインデックス
 *
 * 例: min_block_size=64 の場合
 *   - 64 バイト -> インデックス 0
 *   - 128 バイト -> インデックス 1
 *   - 256 バイト -> インデックス 2
 *   など
 */
inline constexpr std::size_t sizeClassIndex(std::size_t block_size,
                                            std::size_t min_block_size) {
  if (block_size <= min_block_size) {
    return 0;
  }
  return std::countr_zero(std::bit_ceil(block_size)) -
         std::countr_zero(std::bit_ceil(min_block_size));
}

/**
 * @brief min/max ブロックサイズからサイズクラスの総数を計算
 *
 * @param min_block_size 最小ブロックサイズ
 * @param max_block_size 最大ブロックサイズ
 * @return サイズクラスの総数
 *
 * 例: min=64, max=1024 の場合
 *   64, 128, 256, 512, 1024 -> 5 クラス
 */
inline constexpr std::size_t sizeClassCount(std::size_t min_block_size,
                                            std::size_t max_block_size) {
  if (max_block_size < min_block_size || min_block_size == 0) {
    return 0;
  }
  return std::countr_zero(std::bit_ceil(max_block_size)) -
         std::countr_zero(std::bit_ceil(min_block_size)) + 1;
}

/**
 * @brief サイズクラスインデックスからブロックサイズを計算
 *
 * @param index サイズクラスインデックス
 * @param min_block_size 最小ブロックサイズ
 * @return 対応するブロックサイズ
 */
inline constexpr std::size_t sizeClassToBlockSize(std::size_t index,
                                                  std::size_t min_block_size) {
  return std::bit_ceil(min_block_size) << index;
}

} // namespace orteaf::internal::execution::allocator

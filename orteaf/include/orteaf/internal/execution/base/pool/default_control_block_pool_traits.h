#pragma once

#include <cstddef>
#include <cstdint>

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/execution/base/pool/slot_pool.h"

namespace orteaf::internal::execution::base::pool {

// =============================================================================
// Default ControlBlock Handle
// =============================================================================

/**
 * @brief ControlBlock用のHandle型エイリアス
 *
 * タグ型を指定することで、異なるManager間でHandleの混同を防ぐ。
 *
 * @tparam Tag Handle識別用のタグ型（空のstruct推奨）
 */
template <typename Tag>
using ControlBlockHandle =
    ::orteaf::internal::base::Handle<Tag, std::uint32_t, std::uint8_t>;

// =============================================================================
// Default ControlBlock Pool Traits
// =============================================================================

/**
 * @brief ControlBlock Pool用のデフォルトTraits
 *
 * ControlBlockはPoolに格納されるが、create/destroyは何もしない。
 * ControlBlock自体のライフサイクルはPoolが管理し、
 * ControlBlockが管理するPayloadのライフサイクルはControlBlock自身が管理する。
 *
 * @tparam ControlBlockType 使用するControlBlock型
 * @tparam HandleTag Handle識別用のタグ型
 */
template <typename ControlBlockType, typename HandleTag>
struct DefaultControlBlockPoolTraits {
  using Payload = ControlBlockType;
  using Handle = ControlBlockHandle<HandleTag>;

  /// @brief 取得時のリクエスト（未使用）
  struct Request {};

  /// @brief 操作時のコンテキスト（未使用）
  struct Context {};

  /**
   * @brief ControlBlockの作成（何もしない）
   *
   * ControlBlockはデフォルト構築済みで有効な状態。
   */
  static bool create(Payload &, const Request &, const Context &) {
    return true;
  }

  /**
   * @brief ControlBlockの破棄（何もしない）
   *
   * ControlBlockが保持するPayloadの解放は、
   * ControlBlock::release() または Manager::shutdown() が担当。
   */
  static void destroy(Payload &, const Request &, const Context &) {}
};

// =============================================================================
// ControlBlock Pool Type Alias
// =============================================================================

/**
 * @brief ControlBlock Pool型のエイリアス
 *
 * SlotPoolにデフォルトTraitsを適用した型。Managerはこれを使用することで
 * ボイラープレートを削減できる。
 *
 * @tparam ControlBlockType 使用するControlBlock型
 * @tparam HandleTag Handle識別用のタグ型
 */
template <typename ControlBlockType, typename HandleTag>
using ControlBlockPool =
    SlotPool<DefaultControlBlockPoolTraits<ControlBlockType, HandleTag>>;

} // namespace orteaf::internal::execution::base::pool

#pragma once

#include <concepts>
#include <cstddef>

namespace orteaf::internal::base::pool {

// =============================================================================
// 型定義 Concept
// =============================================================================

/// @brief Pool型が必要な基本型を定義しているか
template <typename Pool>
concept PoolTypeConcept = requires {
  typename Pool::Payload;
  typename Pool::Handle;
  typename Pool::SlotRef;
};

// =============================================================================
// 設定 Concept
// =============================================================================

/// @brief configure/shutdown/resize操作をサポート
template <typename Pool>
concept PoolConfigurableConcept =
    PoolTypeConcept<Pool> && requires(Pool &pool, std::size_t n) {
      typename Pool::Config;
      {
        pool.configure(typename Pool::Config{})
      } -> std::convertible_to<std::size_t>;
      { pool.shutdown() };
      { pool.resize(n) } -> std::convertible_to<std::size_t>;
    };

// =============================================================================
// サイズ情報 Concept
// =============================================================================

/// @brief size/capacity取得をサポート
template <typename Pool>
concept PoolSizeQueryConcept = requires(const Pool &pool) {
  { pool.size() } -> std::convertible_to<std::size_t>;
  { pool.capacity() } -> std::convertible_to<std::size_t>;
};

// =============================================================================
// ハンドル検証 Concept
// =============================================================================

/// @brief isValid/isCreatedをサポート
template <typename Pool>
concept HandleValidationConcept =
    PoolTypeConcept<Pool> &&
    requires(const Pool &pool, typename Pool::Handle h) {
      { pool.isValid(h) } -> std::convertible_to<bool>;
      { pool.isCreated(h) } -> std::convertible_to<bool>;
    };

// =============================================================================
// ペイロードアクセス Concept
// =============================================================================

/// @brief handleからPayload*を取得
template <typename Pool>
concept PayloadAccessConcept =
    PoolTypeConcept<Pool> &&
    requires(Pool &pool, const Pool &const_pool, typename Pool::Handle h) {
      { pool.get(h) } -> std::same_as<typename Pool::Payload *>;
      { const_pool.get(h) } -> std::same_as<const typename Pool::Payload *>;
    };

// =============================================================================
// スロット取得 Concept
// =============================================================================

/// @brief 作成済みスロットの取得をサポート
template <typename Pool>
concept CreatedSlotAcquirableConcept =
    PoolTypeConcept<Pool> && requires(Pool &pool) {
      { pool.tryAcquireCreated() } -> std::same_as<typename Pool::SlotRef>;
      { pool.acquireCreated() } -> std::same_as<typename Pool::SlotRef>;
    };

/// @brief 未作成スロットの予約をサポート
template <typename Pool>
concept UncreatedSlotReservableConcept =
    PoolTypeConcept<Pool> && requires(Pool &pool) {
      { pool.tryReserveUncreated() } -> std::same_as<typename Pool::SlotRef>;
      { pool.reserveUncreated() } -> std::same_as<typename Pool::SlotRef>;
    };

// =============================================================================
// スロットリリース Concept
// =============================================================================

/// @brief スロットのリリースをサポート
template <typename Pool>
concept SlotReleasableConcept =
    PoolTypeConcept<Pool> && requires(Pool &pool, typename Pool::Handle h) {
      { pool.release(h) } -> std::convertible_to<bool>;
    };

// =============================================================================
// ペイロード作成 Concept
// =============================================================================

/// @brief emplace/destroyをサポート (Request/Contextが必要)
template <typename Pool>
concept PayloadCreatableConcept =
    PoolTypeConcept<Pool> && requires(Pool &pool, typename Pool::Handle h,
                                      const typename Pool::Request &req,
                                      const typename Pool::Context &ctx) {
      typename Pool::Request;
      typename Pool::Context;
      { pool.emplace(h, req, ctx) } -> std::convertible_to<bool>;
      { pool.destroy(h, req, ctx) } -> std::convertible_to<bool>;
    };

// =============================================================================
// 一括作成 Concept
// =============================================================================

/// @brief createAll/createRangeをサポート
template <typename Pool>
concept BatchCreatableConcept =
    PayloadCreatableConcept<Pool> &&
    requires(Pool &pool, std::size_t start, std::size_t end,
             const typename Pool::Request &req,
             const typename Pool::Context &ctx) {
      { pool.createAll(req, ctx) } -> std::convertible_to<bool>;
      { pool.createRange(start, end, req, ctx) } -> std::convertible_to<bool>;
    };

// =============================================================================
// 組み合わせ Concept
// =============================================================================

/// @brief SlotPoolとFixedSlotStoreが共通で満たすべき最低限
template <typename Pool>
concept BasePoolConcept =
    PoolTypeConcept<Pool> && PoolConfigurableConcept<Pool> &&
    PoolSizeQueryConcept<Pool> && HandleValidationConcept<Pool> &&
    PayloadAccessConcept<Pool>;

/// @brief すべての機能を持つPool
template <typename Pool>
concept FullPoolConcept =
    BasePoolConcept<Pool> && CreatedSlotAcquirableConcept<Pool> &&
    UncreatedSlotReservableConcept<Pool> && SlotReleasableConcept<Pool> &&
    PayloadCreatableConcept<Pool>;

// =============================================================================
// ControlBlockバインディング Concept
// =============================================================================

/// @brief ControlBlockHandle型を定義しているか
template <typename Pool>
concept HasControlBlockHandleConcept =
    requires { typename Pool::ControlBlockHandle; };

/// @brief Payload-ControlBlock間のバインディング追跡をサポート
///
/// Payload HandleからバインドされたControlBlock Handleへの逆引きを可能にする。
/// Library, PipelineState, Heap等のキャッシュされたペイロードを持つPoolで使用。
template <typename Pool>
concept ControlBlockBindableConcept =
    PoolTypeConcept<Pool> && HasControlBlockHandleConcept<Pool> &&
    requires(Pool &pool, const Pool &const_pool, typename Pool::Handle h,
             typename Pool::ControlBlockHandle cbh) {
      // バインドされたCBが存在するか
      { const_pool.hasBoundControlBlock(h) } -> std::convertible_to<bool>;
      // バインドされたCB Handleを取得
      {
        const_pool.getBoundControlBlock(h)
      } -> std::same_as<typename Pool::ControlBlockHandle>;
      // CBをPayloadにバインド
      { pool.bindControlBlock(h, cbh) };
      // CBをアンバインド
      { pool.unbindControlBlock(h) };
    };

// =============================================================================
// バインディング付きPool 組み合わせ Concept
// =============================================================================

/// @brief CB追跡機能付きの完全なPool
template <typename Pool>
concept BoundPoolConcept =
    FullPoolConcept<Pool> && ControlBlockBindableConcept<Pool>;

} // namespace orteaf::internal::base::pool

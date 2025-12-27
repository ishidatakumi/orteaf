#pragma once

#include <concepts>
#include <cstddef>

namespace orteaf::internal::base {

// =============================================================================
// PoolManager 型定義 Concept
// =============================================================================

/// @brief PoolManagerが必要な基本型を定義しているか
template <typename Manager>
concept PoolManagerTypeConcept = requires {
  typename Manager::PayloadPool;
  typename Manager::ControlBlock;
  typename Manager::ControlBlockHandle;
  typename Manager::PayloadHandle;
  typename Manager::WeakLeaseType;
  typename Manager::StrongLeaseType;
  typename Manager::Config;
};

// =============================================================================
// 状態確認 Concept
// =============================================================================

/// @brief 設定状態の確認をサポート
template <typename Manager>
concept ManagerStateQueryConcept =
    PoolManagerTypeConcept<Manager> && requires(const Manager &m) {
      { m.isConfigured() } -> std::convertible_to<bool>;
      { m.ensureConfigured() };
    };

// =============================================================================
// Shutdown チェック Concept
// =============================================================================

/// @brief canShutdown/canTeardownチェックをサポート
template <typename Manager>
concept ManagerShutdownCheckConcept =
    PoolManagerTypeConcept<Manager> && requires(const Manager &m) {
      { m.checkCanShutdownOrThrow() };
      { m.checkCanTeardownOrThrow() };
    };

// =============================================================================
// Block Size 設定 Concept
// =============================================================================

/// @brief block size 設定をサポート
template <typename Manager>
concept ManagerBlockSizeSettableConcept =
    PoolManagerTypeConcept<Manager> && requires(Manager &m, std::size_t n) {
      { m.setControlBlockBlockSize(n) };
      { m.setPayloadBlockSize(n) };
    };

// =============================================================================
// Resize Concept
// =============================================================================

/// @brief resize をサポート
template <typename Manager>
concept ManagerResizableConcept =
    PoolManagerTypeConcept<Manager> && requires(Manager &m, std::size_t n) {
      { m.resizeControlBlockPool(n) } -> std::convertible_to<std::size_t>;
      { m.resizePayloadPool(n) } -> std::convertible_to<std::size_t>;
    };

// =============================================================================
// Growth Chunk Size Concept
// =============================================================================

/// @brief growth chunk size 設定をサポート
template <typename Manager>
concept ManagerGrowthChunkSettableConcept =
    PoolManagerTypeConcept<Manager> &&
    requires(Manager &m, const Manager &cm, std::size_t n) {
      { cm.controlBlockGrowthChunkSize() } -> std::convertible_to<std::size_t>;
      { m.setControlBlockGrowthChunkSize(n) };
      { cm.payloadGrowthChunkSize() } -> std::convertible_to<std::size_t>;
      { m.setPayloadGrowthChunkSize(n) };
    };

// =============================================================================
// Payload 状態確認 Concept
// =============================================================================

/// @brief Payload の生存状態確認をサポート
template <typename Manager>
concept ManagerPayloadAliveCheckConcept =
    PoolManagerTypeConcept<Manager> &&
    requires(const Manager &m, typename Manager::PayloadHandle h) {
      { m.isAlive(h) } -> std::convertible_to<bool>;
    };

// =============================================================================
// Lease 取得 Concept
// =============================================================================

/// @brief WeakLease 取得をサポート
template <typename Manager>
concept ManagerWeakLeaseAcquirableConcept =
    PoolManagerTypeConcept<Manager> &&
    requires(Manager &m, typename Manager::PayloadHandle h) {
      {
        m.acquireWeakLease(h)
      } -> std::same_as<typename Manager::WeakLeaseType>;
    };

/// @brief StrongLease 取得をサポート
template <typename Manager>
concept ManagerStrongLeaseAcquirableConcept =
    PoolManagerTypeConcept<Manager> &&
    requires(Manager &m, typename Manager::PayloadHandle h) {
      {
        m.acquireStrongLease(h)
      } -> std::same_as<typename Manager::StrongLeaseType>;
    };

// =============================================================================
// 組み合わせ Concept
// =============================================================================

/// @brief PoolManager の基本機能（configure 以外）
template <typename Manager>
concept BasePoolManagerConcept =
    ManagerStateQueryConcept<Manager> && ManagerShutdownCheckConcept<Manager> &&
    ManagerBlockSizeSettableConcept<Manager> &&
    ManagerResizableConcept<Manager> &&
    ManagerGrowthChunkSettableConcept<Manager> &&
    ManagerPayloadAliveCheckConcept<Manager>;

/// @brief WeakLease を使う PoolManager
template <typename Manager>
concept WeakLeasePoolManagerConcept =
    BasePoolManagerConcept<Manager> &&
    ManagerWeakLeaseAcquirableConcept<Manager>;

/// @brief StrongLease を使う PoolManager
template <typename Manager>
concept StrongLeasePoolManagerConcept =
    BasePoolManagerConcept<Manager> &&
    ManagerStrongLeaseAcquirableConcept<Manager>;

} // namespace orteaf::internal::base

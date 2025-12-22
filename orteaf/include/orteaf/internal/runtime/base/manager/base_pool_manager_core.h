#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string>

#include <orteaf/internal/diagnostics/error/error.h>
#include <orteaf/internal/runtime/base/pool/default_control_block_pool_traits.h>

namespace orteaf::internal::runtime::base {

// =============================================================================
// Pool Manager Traits Concept
// =============================================================================

/**
 * @brief BasePoolManagerCore用のTraits Concept
 *
 * Required members:
 *   using PayloadPool = ...;         // Payload用のPool型
 *   using ControlBlock = ...;        // ControlBlock型
 *   struct ControlBlockTag {};       // ControlBlock Handle識別用のタグ
 *   using PayloadHandle = ...;       // Payload識別用のHandle型
 *   static constexpr const char* Name = "...";  // エラーメッセージ用の名前
 */
template <typename Traits>
concept PoolManagerTraitsConcept = requires {
  typename Traits::PayloadPool;
  typename Traits::ControlBlock;
  typename Traits::ControlBlockTag;
  typename Traits::PayloadHandle;
  { Traits::Name } -> std::convertible_to<const char *>;
};

// =============================================================================
// BasePoolManagerCore
// =============================================================================

/**
 * @brief 2プール構造（Payload Pool + ControlBlock Pool）の共通処理を提供
 *
 * MpsDeviceManager/MpsEventManager等のManagerに共通する以下の処理を集約：
 * - initialized_ フラグ管理
 * - ensureInitialized() チェック
 * - shutdown時の全ControlBlock canShutdown チェック
 * - ControlBlock Pool の grow
 * - isAlive() 判定
 * - growth_chunk_size_ 管理
 *
 * Managerはこのクラスをコンポジションで使用し、共通処理を委譲する。
 *
 * @tparam Traits PoolManagerTraitsConceptを満たすTraits型
 */
template <typename Traits>
  requires PoolManagerTraitsConcept<Traits>
class BasePoolManagerCore {
public:
  // ===========================================================================
  // Type Aliases
  // ===========================================================================

  using PayloadPool = typename Traits::PayloadPool;
  using ControlBlock = typename Traits::ControlBlock;
  using ControlBlockTag = typename Traits::ControlBlockTag;
  using ControlBlockHandle = pool::ControlBlockHandle<ControlBlockTag>;
  using ControlBlockPoolTraits =
      pool::DefaultControlBlockPoolTraits<ControlBlock, ControlBlockTag>;
  using ControlBlockPool = pool::SlotPool<ControlBlockPoolTraits>;
  using PayloadHandle = typename Traits::PayloadHandle;

  static constexpr const char *managerName() { return Traits::Name; }

  // ===========================================================================
  // Lifecycle
  // ===========================================================================

  BasePoolManagerCore() = default;
  BasePoolManagerCore(const BasePoolManagerCore &) = delete;
  BasePoolManagerCore &operator=(const BasePoolManagerCore &) = delete;
  BasePoolManagerCore(BasePoolManagerCore &&) = default;
  BasePoolManagerCore &operator=(BasePoolManagerCore &&) = default;
  ~BasePoolManagerCore() = default;

  // ===========================================================================
  // Initialization State
  // ===========================================================================

  /**
   * @brief Manager が初期化済みかを返す
   */
  bool isInitialized() const noexcept { return initialized_; }

  /**
   * @brief 初期化済みでなければ例外をスロー
   */
  void ensureInitialized() const {
    if (!initialized_) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          std::string(managerName()) + " has not been initialized");
    }
  }

  /**
   * @brief 初期化済みフラグをセット
   */
  void setInitialized(bool value) noexcept { initialized_ = value; }

  // ===========================================================================
  // ControlBlock Pool Operations
  // ===========================================================================

  /**
   * @brief ControlBlock Pool を初期化
   *
   * @param capacity 初期容量
   */
  void initializeControlBlockPool(std::size_t capacity) {
    typename ControlBlockPoolTraits::Request request{};
    typename ControlBlockPoolTraits::Context context{};
    if (!control_block_pool_.initializeAndCreate(
            typename ControlBlockPoolTraits::Config{capacity}, request,
            context)) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          std::string(managerName()) + " failed to initialize control blocks");
    }
  }

  /**
   * @brief 全ControlBlockに対してcanShutdownチェック
   *
   * 一つでもcanShutdown() == falseのCBがあれば例外をスロー
   */
  void checkCanShutdownOrThrow() const {
    for (std::size_t idx = 0; idx < control_block_pool_.capacity(); ++idx) {
      const ControlBlockHandle handle{static_cast<std::uint32_t>(idx)};
      if (control_block_pool_.isCreated(handle)) {
        const auto *cb = control_block_pool_.get(handle);
        if (cb != nullptr && !cb->canShutdown()) {
          ::orteaf::internal::diagnostics::error::throwError(
              ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
              std::string(managerName()) +
                  " shutdown aborted due to active leases");
        }
      }
    }
  }

  /**
   * @brief ControlBlock Pool を growth_chunk_size_ 分拡張
   */
  void growControlBlockPool() {
    const std::size_t desired =
        control_block_pool_.capacity() + growth_chunk_size_;
    typename ControlBlockPoolTraits::Request request{};
    typename ControlBlockPoolTraits::Context context{};
    control_block_pool_.growAndCreate(
        typename ControlBlockPoolTraits::Config{desired}, request, context);
  }

  /**
   * @brief ControlBlockを取得（なければgrowして再取得）
   *
   * @return ControlBlockへのSlotRef
   * @throws OutOfRange grow後も取得できない場合
   */
  typename ControlBlockPool::SlotRef acquireControlBlock() {
    typename ControlBlockPoolTraits::Request request{};
    typename ControlBlockPoolTraits::Context context{};
    auto ref = control_block_pool_.tryAcquire(request, context);
    if (!ref.valid()) {
      growControlBlockPool();
      ref = control_block_pool_.tryAcquire(request, context);
    }
    if (!ref.valid()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::OutOfRange,
          std::string(managerName()) + " has no available control blocks");
    }
    return ref;
  }

  /**
   * @brief ControlBlockをプールに返却
   *
   * @param handle 返却するCBのHandle
   */
  void releaseControlBlock(ControlBlockHandle handle) noexcept {
    control_block_pool_.release(handle);
  }

  /**
   * @brief ControlBlock Poolをshutdown
   */
  void shutdownControlBlockPool() { control_block_pool_.shutdown(); }

  // ===========================================================================
  // Growth Chunk Size Configuration
  // ===========================================================================

  /**
   * @brief Pool拡張時のチャンクサイズを取得
   */
  std::size_t growthChunkSize() const noexcept { return growth_chunk_size_; }

  /**
   * @brief Pool拡張時のチャンクサイズを設定
   *
   * @param size チャンクサイズ（0より大きい必要がある）
   */
  void setGrowthChunkSize(std::size_t size) {
    if (size == 0) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          std::string(managerName()) + " growth chunk size must be > 0");
    }
    growth_chunk_size_ = size;
  }

  // ===========================================================================
  // Accessors
  // ===========================================================================

  /**
   * @brief ControlBlock Poolへのアクセス
   */
  ControlBlockPool &controlBlockPool() noexcept { return control_block_pool_; }
  const ControlBlockPool &controlBlockPool() const noexcept {
    return control_block_pool_;
  }

  /**
   * @brief Payload Poolへのアクセス
   */
  PayloadPool &payloadPool() noexcept { return payload_pool_; }
  const PayloadPool &payloadPool() const noexcept { return payload_pool_; }

  // ===========================================================================
  // isAlive Helper
  // ===========================================================================

  /**
   * @brief Payload Handleが有効で作成済みかを判定
   *
   * @param handle チェック対象のHandle
   * @return 初期化済み && valid && created であればtrue
   */
  bool isAlive(PayloadHandle handle) const noexcept {
    return initialized_ && payload_pool_.isValid(handle) &&
           payload_pool_.isCreated(handle);
  }

#if ORTEAF_ENABLE_TEST
  // ===========================================================================
  // Test Support
  // ===========================================================================

  std::size_t controlBlockPoolCapacityForTest() const noexcept {
    return control_block_pool_.capacity();
  }

  std::size_t controlBlockPoolAvailableForTest() const noexcept {
    return control_block_pool_.available();
  }
#endif

private:
  bool initialized_{false};
  std::size_t growth_chunk_size_{1};
  PayloadPool payload_pool_{};
  ControlBlockPool control_block_pool_{};
};

} // namespace orteaf::internal::runtime::base

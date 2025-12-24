#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string>

#include <orteaf/internal/diagnostics/error/error.h>
#include <orteaf/internal/execution/base/pool/default_control_block_pool_traits.h>

namespace orteaf::internal::execution::base {

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

  struct Config {
    std::size_t control_block_capacity{0};
    std::size_t control_block_block_size{0};
    std::size_t growth_chunk_size{1};
  };

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
   * @brief Core設定を適用
   *
   * @param config Core全体の設定
   */
  void configure(const Config &config) {
    applyControlBlockConfig(config);
    setGrowthChunkSize(config.growth_chunk_size);
  }

  /**
   * @brief 全ControlBlockに対してcanShutdownチェック
   *
   * 一つでもcanShutdown() == falseのCBがあれば例外をスロー
   */
  void checkCanShutdownOrThrow() const {
    control_block_pool_.forEachCreated([&](std::size_t,
                                           const ControlBlock &cb) {
      if (!cb.canShutdown()) {
        ::orteaf::internal::diagnostics::error::throwError(
            ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
            std::string(managerName()) +
                " shutdown aborted due to active leases");
      }
    });
  }

  /**
   * @brief ControlBlock Pool を growth_chunk_size_ 分拡張
   */
  void growControlBlockPool() {
    if (control_block_block_size_ == 0) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          std::string(managerName()) + " control block size is not set");
    }
    const std::size_t desired = control_block_pool_.size() + growth_chunk_size_;
    typename ControlBlockPoolTraits::Request request{};
    typename ControlBlockPoolTraits::Context context{};
    const std::size_t old_capacity = control_block_pool_.resize(desired);
    control_block_pool_.createRange(old_capacity, control_block_pool_.size(),
                                    request, context);
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
    auto ref = control_block_pool_.tryAcquireCreated(request, context);
    if (!ref.valid()) {
      growControlBlockPool();
      ref = control_block_pool_.tryAcquireCreated(request, context);
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
   * @brief ControlBlock Pool for lease construction only.
   */
  ControlBlockPool *controlBlockPoolForLease() noexcept {
    return &control_block_pool_;
  }

  /**
   * @brief Payload Poolへのアクセス
   */
  PayloadPool &payloadPool() noexcept { return payload_pool_; }
  const PayloadPool &payloadPool() const noexcept { return payload_pool_; }

  /**
   * @brief Payload Pool を指定量だけ拡張
   *
   * @param grow_by 追加で確保するスロット数
   * @return 拡張後の容量
   */
  std::size_t growPayloadPoolBy(std::size_t grow_by) {
    if (grow_by == 0) {
      return payload_pool_.size();
    }
    const std::size_t desired = payload_pool_.size() + grow_by;
    payload_pool_.resize(desired);
    return desired;
  }

  /**
   * @brief Payload Pool を拡張し、新規領域を作成済みにする
   *
   * @param grow_by 追加で確保するスロット数
   * @param request Payload作成に渡すリクエスト
   * @param context Payload作成に渡すコンテキスト
   * @return 作成が成功した場合はtrue
   */
  template <typename Request, typename Context>
  bool growPayloadPoolByAndCreate(std::size_t grow_by, const Request &request,
                                  const Context &context)
    requires requires(PayloadPool &pool, std::size_t start, std::size_t end,
                      const Request &req, const Context &ctx) {
      { pool.createRange(start, end, req, ctx) } -> std::convertible_to<bool>;
    }
  {
    const std::size_t old_size = payload_pool_.size();
    const std::size_t new_size = growPayloadPoolBy(grow_by);
    if (new_size == old_size) {
      return true;
    }
    return payload_pool_.createRange(old_size, payload_pool_.size(), request,
                                     context);
  }

  /**
   * @brief Payload Pool から作成済みスロットを取得（必要なら拡張＋作成）
   *
   * @param grow_by 追加で確保するスロット数
   * @param request Payload作成に渡すリクエスト
   * @param context Payload作成に渡すコンテキスト
   * @return 取得できなければ invalid な SlotRef
   */
  template <typename Request, typename Context>
  typename PayloadPool::SlotRef acquirePayloadOrGrowAndCreate(
      std::size_t grow_by, const Request &request, const Context &context)
    requires requires(PayloadPool &pool, const Request &req,
                      const Context &ctx) {
      {
        pool.tryAcquireCreated(req, ctx)
      } -> std::same_as<typename PayloadPool::SlotRef>;
    }
  {
    auto ref = payload_pool_.tryAcquireCreated(request, context);
    if (ref.valid()) {
      return ref;
    }
    if (!growPayloadPoolByAndCreate(grow_by, request, context)) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          std::string(managerName()) + " failed to create payloads");
    }
    return payload_pool_.tryAcquireCreated(request, context);
  }

  /**
   * @brief 未作成スロットを予約（必要なら拡張）
   *
   * @param grow_by 追加で確保するスロット数
   * @param request 予約に使うリクエスト
   * @param context 予約に使うコンテキスト
   * @return 取得できなければ invalid な SlotRef
   */
  template <typename Request, typename Context>
  typename PayloadPool::SlotRef reserveUncreatedPayloadOrGrow(
      std::size_t grow_by, const Request &request, const Context &context)
    requires requires(PayloadPool &pool, const Request &req,
                      const Context &ctx) {
      {
        pool.tryReserveUncreated(req, ctx)
      } -> std::same_as<typename PayloadPool::SlotRef>;
    }
  {
    auto ref = payload_pool_.tryReserveUncreated(request, context);
    if (ref.valid() || grow_by == 0) {
      return ref;
    }
    growPayloadPoolBy(grow_by);
    return payload_pool_.tryReserveUncreated(request, context);
  }

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

  std::size_t controlBlockPoolSizeForTest() const noexcept {
    return control_block_pool_.size();
  }

  std::size_t controlBlockPoolCapacityForTest() const noexcept {
    return control_block_pool_.capacity();
  }

  std::size_t controlBlockPoolAvailableForTest() const noexcept {
    return control_block_pool_.available();
  }
#endif

private:
  void applyControlBlockConfig(const Config &config) {
    const std::size_t capacity = config.control_block_capacity;
    const std::size_t block_size = config.control_block_block_size;
    if (block_size == 0) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          std::string(managerName()) + " control block size must be > 0");
    }
    if (control_block_block_size_ != 0 &&
        control_block_block_size_ != block_size) {
      checkCanShutdownOrThrow();
      control_block_pool_.shutdown();
    }
    control_block_block_size_ = block_size;
    typename ControlBlockPoolTraits::Request request{};
    typename ControlBlockPoolTraits::Context context{};
    const std::size_t old_capacity = control_block_pool_.configure(
        typename ControlBlockPool::Config{capacity, block_size});
    if (!control_block_pool_.createRange(
            old_capacity, control_block_pool_.size(), request, context)) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          std::string(managerName()) + " failed to initialize control blocks");
    }
  }

  bool initialized_{false};
  std::size_t growth_chunk_size_{1};
  std::size_t control_block_block_size_{0};
  PayloadPool payload_pool_{};
  ControlBlockPool control_block_pool_{};
};

} // namespace orteaf::internal::execution::base

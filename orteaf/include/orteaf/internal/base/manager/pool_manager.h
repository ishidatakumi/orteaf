#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

#include <orteaf/internal/base/lease/concepts.h>
#include <orteaf/internal/base/lease/strong_lease.h>
#include <orteaf/internal/base/lease/weak_lease.h>
#include <orteaf/internal/base/pool/default_control_block_pool_traits.h>
#include <orteaf/internal/base/pool/pool_concepts.h>
#include <orteaf/internal/diagnostics/error/error.h>

namespace orteaf::internal::base {

// =============================================================================
// Pool Manager Traits Concept
// =============================================================================

/**
 * @brief PoolManager用のTraits Concept
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
// PoolManager
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
 * - control_block_growth_chunk_size_ 管理
 *
 * Managerはこのクラスをコンポジションで使用し、共通処理を委譲する。
 *
 * @tparam Traits PoolManagerTraitsConceptを満たすTraits型
 */
template <typename Traits>
  requires PoolManagerTraitsConcept<Traits>
class PoolManager {
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

  // Lease types - PoolManager is the friend (ManagerT) for these leases
  using WeakLeaseType = WeakLease<ControlBlockHandle, ControlBlock,
                                  ControlBlockPool, PoolManager<Traits>>;
  using StrongLeaseType = StrongLease<ControlBlockHandle, ControlBlock,
                                      ControlBlockPool, PoolManager<Traits>>;

  static constexpr const char *managerName() { return Traits::Name; }

  struct Config {
    std::size_t control_block_capacity{0};
    std::size_t control_block_block_size{0};
    std::size_t control_block_growth_chunk_size{1};
    std::size_t payload_growth_chunk_size{1};
    std::size_t payload_capacity{0};
    std::size_t payload_block_size{0};
  };

  // ===========================================================================
  // Lifecycle
  // ===========================================================================

  PoolManager() = default;
  PoolManager(const PoolManager &) = delete;
  PoolManager &operator=(const PoolManager &) = delete;
  PoolManager(PoolManager &&) = default;
  PoolManager &operator=(PoolManager &&) = default;
  ~PoolManager() = default;

  // ===========================================================================
  // Configuration State
  // ===========================================================================

  /**
   * @brief Manager が設定済みかを返す
   */
  bool isConfigured() const noexcept {
    return configured_;
  }

  /**
   * @brief 設定済みでなければ例外をスロー
   */
  void ensureConfigured() const {
    if (!isConfigured()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          std::string(managerName()) + " has not been configured");
    }
  }

  // ===========================================================================
  // Configuration
  // ===========================================================================

  template <typename Request, typename Context>
  void configure(const Config &config, const Request &request,
                 const Context &context)
    requires requires(PayloadPool &pool, std::size_t capacity,
                      std::size_t block_size, const Request &req,
                      const Context &ctx) {
      pool.setBlockSize(block_size);
      pool.resize(capacity);
      pool.clear(req, ctx);
    }
  {
    validateConfig(config);

    // Payload block size 変更時は shutdown が必要
    if (isConfigured() && payload_block_size_ != config.payload_block_size &&
        config.payload_block_size != 0) {
      shutdownPayloadPool(request, context);
    }

    // Payload の設定
    setPayloadBlockSize(config.payload_block_size);
    resizePayloadPool(config.payload_capacity);

    // ControlBlock の設定
    applyControlBlockConfig(config);

    // Growth chunk sizes
    setControlBlockGrowthChunkSize(config.control_block_growth_chunk_size);
    setPayloadGrowthChunkSize(config.payload_growth_chunk_size);

    configured_ = true;
  }

  // ===========================================================================
  // Shutdown
  // ===========================================================================

  /**
   * @brief Manager全体をshutdown
   *
   * 以下の順序で安全にshutdownを行う:
   * 1. checkCanTeardownOrThrow() - Payload解放可能かチェック
   * 2. payload_pool_.clear() - Payload Pool クリア
   * 3. checkCanShutdownOrThrow() - ControlBlock解放可能かチェック
   * 4. control_block_pool_.clear() - ControlBlock Pool クリア
   *
   * @param request Payload clear用のリクエスト
   * @param context Payload clear用のコンテキスト
   */
  template <typename Request, typename Context>
  void shutdown(const Request &request, const Context &context)
    requires requires(PayloadPool &pool, const Request &req,
                      const Context &ctx) { pool.clear(req, ctx); }
  {
    if (!isConfigured()) {
      return;
    }

    shutdownPayloadPool(request, context);
    shutdownControlBlockPool();

    configured_ = false;
  }

  // ===========================================================================
  // Block Size Setters
  // ===========================================================================

  /**
   * @brief ControlBlock Pool の block size を設定
   *
   * block size の変更が必要な場合、canShutdown チェックを行い
   * 既存の ControlBlock を shutdown してから設定する。
   *
   * @param size ブロックサイズ（0より大きい必要がある）
   */
  void setControlBlockBlockSize(std::size_t size) {
    if (size == 0) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          std::string(managerName()) + " control block size must be > 0");
    }
    if (control_block_block_size_ != 0 && control_block_block_size_ != size) {
      checkCanShutdownOrThrow();
    }
    control_block_block_size_ = size;
    control_block_pool_.setBlockSize(size);
  }

  /**
   * @brief Payload Pool の block size を設定
   *
   * block size の変更が必要な場合、canTeardown チェックを行う。
   * 実際の shutdown は呼び出し側で行う必要がある。
   *
   * @param size ブロックサイズ（0より大きい必要がある）
   */
  void setPayloadBlockSize(std::size_t size) {
    if (size == 0) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          std::string(managerName()) + " payload block size must be > 0");
    }
    if (payload_block_size_ != 0 && payload_block_size_ != size) {
      checkCanTeardownOrThrow();
    }
    payload_block_size_ = size;
    payload_pool_.setBlockSize(size);
  }

  // ===========================================================================
  // Growth Chunk Size Setters
  // ===========================================================================

  /**
   * @brief ControlBlock Pool拡張時のチャンクサイズを取得
   */
  std::size_t controlBlockGrowthChunkSize() const noexcept {
    return control_block_growth_chunk_size_;
  }

  /**
   * @brief ControlBlock Pool拡張時のチャンクサイズを設定
   *
   * @param size チャンクサイズ（0より大きい必要がある）
   */
  void setControlBlockGrowthChunkSize(std::size_t size) {
    if (size == 0) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          std::string(managerName()) +
              " control block growth chunk size must be > 0");
    }
    control_block_growth_chunk_size_ = size;
  }

  /**
   * @brief Payload Pool拡張時のチャンクサイズを取得
   */
  std::size_t payloadGrowthChunkSize() const noexcept {
    return payload_growth_chunk_size_;
  }

  /**
   * @brief Payload Pool拡張時のチャンクサイズを設定
   *
   * @param size チャンクサイズ（0より大きい必要がある）
   */
  void setPayloadGrowthChunkSize(std::size_t size) {
    if (size == 0) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          std::string(managerName()) +
              " payload growth chunk size must be > 0");
    }
    payload_growth_chunk_size_ = size;
  }

  // ===========================================================================
  // Payload Operations
  // ===========================================================================

  /**
   * @brief Payload Handleが有効で作成済みかを判定
   *
   * @param handle チェック対象のHandle
   * @return 初期化済み && valid && created であればtrue
   */
  bool isAlive(PayloadHandle handle) const noexcept {
    return isConfigured() && payload_pool_.isValid(handle) &&
           payload_pool_.isCreated(handle);
  }

  /**
   * @brief Payload Poolの全作成
   */
  template <typename Request, typename Context>
  bool createAllPayloads(const Request &request, const Context &context)
    requires requires(PayloadPool &pool, const Request &req,
                      const Context &ctx) {
      { pool.createAll(req, ctx) } -> std::convertible_to<bool>;
    }
  {
    return payload_pool_.createAll(request, context);
  }

  /**
   * @brief Payloadを指定ハンドルに作成
   */
  template <typename Request, typename Context>
  bool emplacePayload(PayloadHandle handle, const Request &request,
                      const Context &context)
    requires requires(PayloadPool &pool, PayloadHandle h, const Request &req,
                      const Context &ctx) {
      { pool.emplace(h, req, ctx) } -> std::convertible_to<bool>;
    }
  {
    return payload_pool_.emplace(handle, request, context);
  }

  /**
   * @brief Payload Pool から作成済みスロットを取得（必要なら拡張＋作成）
   *
   * @param grow_by 追加で確保するスロット数
   * @param request Payload作成に渡すリクエスト
   * @param context Payload作成に渡すコンテキスト
   * @return 取得できなければ invalid な Handle
   */
  template <typename Request, typename Context>
  PayloadHandle acquirePayloadOrGrowAndCreate(const Request &request,
                                              const Context &context)
    requires requires(PayloadPool &pool) {
      { pool.tryAcquireCreated() } -> std::same_as<PayloadHandle>;
    }
  {
    auto handle = payload_pool_.tryAcquireCreated();
    if (handle.isValid()) {
      return handle;
    }
    if (!growPayloadPoolByAndCreate(payload_growth_chunk_size_, request,
                                    context)) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          std::string(managerName()) + " failed to create payloads");
    }
    return payload_pool_.tryAcquireCreated();
  }

  /**
   * @brief 未作成スロットを予約（必要なら拡張）
   *
   * @param grow_by 追加で確保するスロット数
   * @return 取得できなければ invalid な Handle
   */
  PayloadHandle reserveUncreatedPayloadOrGrow()
    requires requires(PayloadPool &pool) {
      { pool.tryReserveUncreated() } -> std::same_as<PayloadHandle>;
    }
  {
    auto handle = payload_pool_.tryReserveUncreated();
    if (handle.isValid()) {
      return handle;
    }
    growPayloadPoolBy(payload_growth_chunk_size_);
    return payload_pool_.tryReserveUncreated();
  }

  // ===========================================================================
  // Lease Factory (High-Level API)
  // ===========================================================================

  /**
   * @brief WeakLeaseを取得（Binding対応Poolの場合は既存CBを再利用）
   *
   * @param handle Payload handle
   * @return WeakLeaseType
   * @throws InvalidArgument handleが無効な場合
   * @throws InvalidState payloadが利用不可の場合
   * @note ControlBlockがWeakControlBlockConceptを満たす場合のみ利用可能
   */
  WeakLeaseType acquireWeakLease(PayloadHandle handle)
    requires WeakControlBlockConcept<ControlBlock>
  {
    return acquireLeaseImpl<WeakLeaseType>(handle);
  }

  /**
   * @brief StrongLeaseを取得（Binding対応Poolの場合は既存CBを再利用）
   *
   * @param handle Payload handle
   * @return StrongLeaseType
   * @throws InvalidArgument handleが無効な場合
   * @throws InvalidState payloadが利用不可の場合
   * @note ControlBlockがStrongControlBlockConceptを満たす場合のみ利用可能
   */
  StrongLeaseType acquireStrongLease(PayloadHandle handle)
    requires StrongControlBlockConcept<ControlBlock>
  {
    return acquireLeaseImpl<StrongLeaseType>(handle);
  }

#if ORTEAF_ENABLE_TEST
  // ===========================================================================
  // Test Support
  // ===========================================================================

  std::size_t payloadPoolSizeForTest() const noexcept
    requires requires(const PayloadPool &pool) { pool.size(); }
  {
    return payload_pool_.size();
  }

  std::size_t payloadPoolCapacityForTest() const noexcept
    requires requires(const PayloadPool &pool) { pool.capacity(); }
  {
    return payload_pool_.capacity();
  }

  std::size_t payloadPoolAvailableForTest() const noexcept
    requires requires(const PayloadPool &pool) { pool.available(); }
  {
    return payload_pool_.available();
  }

  bool payloadCreatedForTest(PayloadHandle handle) const noexcept
    requires requires(const PayloadPool &pool, PayloadHandle h) {
      pool.isCreated(h);
    }
  {
    return payload_pool_.isCreated(handle);
  }

  auto payloadForTest(PayloadHandle handle) const noexcept
      -> decltype(std::declval<const PayloadPool &>().get(handle))
    requires requires(const PayloadPool &pool, PayloadHandle h) { pool.get(h); }
  {
    return payload_pool_.get(handle);
  }

  auto boundControlBlockForTest(PayloadHandle handle) const noexcept
    requires requires(const PayloadPool &pool, PayloadHandle h) {
      pool.getBoundControlBlock(h);
    }
  {
    return payload_pool_.getBoundControlBlock(handle);
  }

  std::size_t controlBlockPoolSizeForTest() const noexcept {
    return control_block_pool_.size();
  }

  std::size_t controlBlockPoolCapacityForTest() const noexcept {
    return control_block_pool_.capacity();
  }

  std::size_t controlBlockPoolAvailableForTest() const noexcept {
    return control_block_pool_.available();
  }

  const ControlBlock *
  controlBlockForTest(ControlBlockHandle handle) const noexcept {
    return control_block_pool_.get(handle);
  }
#endif

private:
  // ===========================================================================
  // Configuration Helpers
  // ===========================================================================

  static void validateConfig(const Config &config) {
    if (config.control_block_block_size == 0) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          std::string(managerName()) + " control block size must be > 0");
    }
    if (config.control_block_growth_chunk_size == 0) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          std::string(managerName()) +
              " control block growth chunk size must be > 0");
    }
    if (config.payload_growth_chunk_size == 0) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          std::string(managerName()) +
              " payload growth chunk size must be > 0");
    }
    if (config.payload_capacity != 0 && config.payload_block_size == 0) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          std::string(managerName()) + " payload block size must be > 0");
    }
  }

  void configureCheckedNoShutdown(const Config &config)
    requires requires(PayloadPool &pool, std::size_t capacity,
                      std::size_t block_size) {
      pool.setBlockSize(block_size);
      pool.resize(capacity);
    }
  {
    validateConfig(config);

    // Payload の設定 (setPayloadBlockSize 内で canTeardown チェック)
    setPayloadBlockSize(config.payload_block_size);
    resizePayloadPool(config.payload_capacity);

    // ControlBlock の設定
    applyControlBlockConfig(config);

    // Growth chunk sizes
    setControlBlockGrowthChunkSize(config.control_block_growth_chunk_size);
    setPayloadGrowthChunkSize(config.payload_growth_chunk_size);
  }

  void applyControlBlockConfig(const Config &config) {
    // block size 設定 (内部で canShutdown チェックと shutdown 実行)
    setControlBlockBlockSize(config.control_block_block_size);

    // resize してから createRange
    const std::size_t old_capacity =
        resizeControlBlockPool(config.control_block_capacity);
    typename ControlBlockPoolTraits::Request request{};
    typename ControlBlockPoolTraits::Context context{};
    if (!control_block_pool_.createRange(
            old_capacity, control_block_pool_.size(), request, context)) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          std::string(managerName()) + " failed to initialize control blocks");
    }
  }

  // ===========================================================================
  // Resize Helpers
  // ===========================================================================

  /**
   * @brief ControlBlock Pool の容量をリサイズ
   *
   * @param capacity 新しい容量
   * @return リサイズ前の容量
   */
  std::size_t resizeControlBlockPool(std::size_t capacity) {
    return control_block_pool_.resize(capacity);
  }

  /**
   * @brief Payload Pool の容量をリサイズ
   *
   * @param capacity 新しい容量
   * @return リサイズ前の容量
   */
  std::size_t resizePayloadPool(std::size_t capacity) {
    return payload_pool_.resize(capacity);
  }

  // ===========================================================================
  // Shutdown Helpers
  // ===========================================================================

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
   * @brief 全ControlBlockに対してcanTeardownチェック
   *
   * Payload を破棄可能かどうかをチェック。
   * 一つでもcanTeardown() == falseのCBがあれば例外をスロー
   */
  void checkCanTeardownOrThrow() const {
    control_block_pool_.forEachCreated([&](std::size_t,
                                           const ControlBlock &cb) {
      if (!cb.canTeardown()) {
        ::orteaf::internal::diagnostics::error::throwError(
            ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
            std::string(managerName()) +
                " teardown aborted due to active strong references");
      }
    });
  }

  /**
   * @brief Payload Poolをclear
   */
  template <typename Request, typename Context>
  void shutdownPayloadPool(const Request &request, const Context &context)
    requires requires(PayloadPool &pool, const Request &req,
                      const Context &ctx) { pool.clear(req, ctx); }
  {
    checkCanTeardownOrThrow();
    payload_pool_.clear(request, context);
  }

  /**
   * @brief ControlBlock Poolをshutdown
   */
  void shutdownControlBlockPool() {
    checkCanShutdownOrThrow();
    control_block_pool_.clear();
  }

  // ===========================================================================
  // Resize / Growth Helpers
  // ===========================================================================

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
   * @brief ControlBlock Pool を control_block_growth_chunk_size_ 分拡張
   */
  void growControlBlockPool() {
    if (control_block_block_size_ == 0) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          std::string(managerName()) + " control block size is not set");
    }
    const std::size_t desired =
        control_block_pool_.size() + control_block_growth_chunk_size_;
    typename ControlBlockPoolTraits::Request request{};
    typename ControlBlockPoolTraits::Context context{};
    const std::size_t old_capacity = control_block_pool_.resize(desired);
    control_block_pool_.createRange(old_capacity, control_block_pool_.size(),
                                    request, context);
  }

  // ===========================================================================
  // ControlBlock Acquisition
  // ===========================================================================

  /**
   * @brief ControlBlockを取得（なければgrowして再取得）
   *
   * @return ControlBlockHandle
   * @throws OutOfRange grow後も取得できない場合
   */
  ControlBlockHandle acquireControlBlock() {
    auto handle = control_block_pool_.tryAcquireCreated();
    if (!handle.isValid()) {
      growControlBlockPool();
      handle = control_block_pool_.tryAcquireCreated();
    }
    if (!handle.isValid()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::OutOfRange,
          std::string(managerName()) + " has no available control blocks");
    }
    return handle;
  }

  // ===========================================================================
  // ControlBlock Access
  // ===========================================================================

  /**
   * @brief Get control block pointer from handle
   *
   * @param handle ControlBlock handle
   * @return Pointer to ControlBlock, or nullptr if invalid
   */
  ControlBlock *getControlBlock(ControlBlockHandle handle) noexcept {
    return control_block_pool_.get(handle);
  }

  /**
   * @brief Get control block pointer from handle (const version)
   *
   * @param handle ControlBlock handle
   * @return Const pointer to ControlBlock, or nullptr if invalid
   */
  const ControlBlock *
  getControlBlock(ControlBlockHandle handle) const noexcept {
    return control_block_pool_.get(handle);
  }

  /**
   * @brief ControlBlockをプールに返却
   *
   * @param handle 返却するCBのHandle
   */
  void releaseControlBlock(ControlBlockHandle handle) noexcept {
    control_block_pool_.release(handle);
  }

  // ===========================================================================
  // Lease Helpers
  // ===========================================================================

  /**
   * @brief Lease取得の共通実装
   *
   * @tparam LeaseType WeakLeaseType または StrongLeaseType
   * @param handle Payload handle
   * @return LeaseType
   * @throws InvalidArgument handleが無効な場合
   * @throws InvalidState payloadが利用不可の場合
   */
  template <typename LeaseType>
  LeaseType acquireLeaseImpl(PayloadHandle handle) {
    ensureConfigured();

    // Validate handle
    if (!payload_pool_.isValid(handle)) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          std::string(managerName()) + " handle is invalid");
    }
    if (!payload_pool_.isCreated(handle)) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          std::string(managerName()) + " payload is unavailable");
    }

    auto *payload_ptr = payload_pool_.get(handle);
    if (payload_ptr == nullptr) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          std::string(managerName()) + " payload pointer is null");
    }

    // Check for existing bound CB (if pool supports binding)
    if constexpr (pool::ControlBlockBindableConcept<PayloadPool>) {
      if (payload_pool_.hasBoundControlBlock(handle)) {
        auto existing_cb_handle = payload_pool_.getBoundControlBlock(handle);
        auto *cb_ptr = control_block_pool_.get(existing_cb_handle);
        if (cb_ptr != nullptr) {
          if constexpr (std::is_same_v<LeaseType, WeakLeaseType>) {
            cb_ptr->acquireWeak();
          } else {
            cb_ptr->acquireStrong();
          }
          return LeaseType{cb_ptr, &control_block_pool_, existing_cb_handle};
        }
        payload_pool_.unbindControlBlock(handle);
      }
    }

    // Acquire new CB
    auto cb_handle = acquireControlBlock();
    auto *cb_ptr = getControlBlock(cb_handle);
    if (!cb_ptr->tryBindPayload(handle, payload_ptr, &payload_pool_)) {
      releaseControlBlock(cb_handle);
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          std::string(managerName()) + " control block binding failed");
    }

    // Bind CB handle to payload (if pool supports binding)
    if constexpr (pool::ControlBlockBindableConcept<PayloadPool>) {
      payload_pool_.bindControlBlock(handle, cb_handle);
    }

    return LeaseType{cb_ptr, &control_block_pool_, cb_handle};
  }

  // ===========================================================================
  // State
  // ===========================================================================

  std::size_t control_block_growth_chunk_size_{1};
  std::size_t payload_growth_chunk_size_{1};
  std::size_t control_block_block_size_{0};
  std::size_t payload_block_size_{0};
  bool configured_{false};
  PayloadPool payload_pool_{};
  ControlBlockPool control_block_pool_{};
};

} // namespace orteaf::internal::base

#pragma once

#include <cstddef>
#include <cstdint>

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/diagnostics/error/error_macros.h"
#include "orteaf/internal/execution/allocator/buffer_resource.h"
#include "orteaf/internal/execution/allocator/policies/policy_config.h"

namespace orteaf::internal::execution::allocator::policies {

/**
 * @brief Direct スタイルの ChunkLocator ポリシー。
 *
 * BufferViewHandle の上位ビットで large/chunk
 * を判別し、下位ビットをチャンクのスロットに割り当てる。 device/context
 * ごとに分けず、配列ひとつで O(1) アクセスにする。
 *
 * @tparam Resource リソース管理クラス
 */
template <typename Resource>
class DirectChunkLocatorPolicy {
public:
  // ========================================================================
  // Type aliases
  // ========================================================================
  using BufferViewHandle = ::orteaf::internal::base::BufferViewHandle;
  using BufferView = typename Resource::BufferView;
  using BufferBlock = Resource::BufferBlock;

  DirectChunkLocatorPolicy() = default;
  DirectChunkLocatorPolicy(const DirectChunkLocatorPolicy &) = delete;
  DirectChunkLocatorPolicy &
  operator=(const DirectChunkLocatorPolicy &) = delete;
  DirectChunkLocatorPolicy(DirectChunkLocatorPolicy &&) = default;
  DirectChunkLocatorPolicy &operator=(DirectChunkLocatorPolicy &&) = default;
  ~DirectChunkLocatorPolicy() = default;

  /**
   * @brief DirectChunkLocatorPolicy 固有の設定。
   */
  struct Config : PolicyConfig<Resource> {};

  // ========================================================================
  // Public API
  // ========================================================================

  /**
   * @brief ポリシーを初期化する。
   * @param config 設定
   */
  void initialize(const Config &config) {
    ORTEAF_THROW_IF_NULL(
        config.resource,
        "DirectChunkLocatorPolicy requires non-null Resource*");
    config_ = config;
    resource_ = config.resource;
  }

  /**
   * @brief チャンクを確保して登録し、対応する BufferBlock を返す。
   * @param size 確保サイズ
   * @param alignment アラインメント
   * @return 確保された BufferBlock（失敗時は空）
   */
  BufferBlock addChunk(std::size_t size, std::size_t alignment) {

    ORTEAF_THROW_IF(resource_ == nullptr, InvalidState,
                    "DirectChunkLocatorPolicy is not initialized");
    ORTEAF_THROW_IF(size == 0, InvalidParameter, "size must be non-zero");

    BufferView base = resource_->allocate(size, alignment);
    if (!base) {
      return {};
    }

    const std::size_t slot = reserveSlot();
    chunks_[slot] = ChunkInfo{base, size, alignment, 0u, 0u, true};
    return BufferBlock{encodeId(slot), base};
  }

  /**
   * @brief チャンク全体を解放する（used/pending が 0 のときのみ）。
   * @param id 解放するチャンクの BufferViewHandle
   * @return 解放に成功した場合 true
   */
  bool releaseChunk(BufferViewHandle handle) {

    const std::size_t slot = indexFromId(handle);
    if (slot >= chunks_.size() || resource_ == nullptr) {
      return false;
    }

    ChunkInfo &chunk = chunks_[slot];
    if (!chunk.alive || chunk.used != 0 || chunk.pending != 0) {
      return false;
    }

    resource_->deallocate(chunk.base, chunk.size, chunk.alignment);
    chunk = ChunkInfo{};
    free_list_.pushBack(slot);
    return true;
  }

  /**
   * @brief チャンクサイズを取得する。
   * @param id チャンクの BufferViewHandle
   * @return チャンクサイズ（無効な場合 0）
   */
  std::size_t findChunkSize(BufferViewHandle handle) const {

    const ChunkInfo *chunk = find(handle);
    return chunk ? chunk->size : 0;
  }

  BufferViewHandle findReleasable() const {

    for (std::size_t i = 0; i < chunks_.size(); ++i) {
      const ChunkInfo &chunk = chunks_[i];
      if (chunk.alive && chunk.used == 0 && chunk.pending == 0) {
        return encodeId(i);
      }
    }
    return BufferViewHandle::invalid();
  }

  void incrementUsed(BufferViewHandle handle) {

    if (auto *chunk = find(handle)) {
      ++chunk->used;
    }
  }

  void decrementUsed(BufferViewHandle handle) {

    if (auto *chunk = find(handle)) {
      if (chunk->used > 0) {
        --chunk->used;
      }
    }
  }

  void incrementPending(BufferViewHandle handle) {

    if (auto *chunk = find(handle)) {
      ++chunk->pending;
    }
  }

  void decrementPending(BufferViewHandle handle) {

    if (auto *chunk = find(handle)) {
      if (chunk->pending > 0) {
        --chunk->pending;
      }
    }
  }

  void decrementPendingAndUsed(BufferViewHandle handle) {

    if (auto *chunk = find(handle)) {
      if (chunk->pending > 0) {
        --chunk->pending;
      }
      if (chunk->used > 0) {
        --chunk->used;
      }
    }
  }

  /**
   * @brief チャンクが有効かどうかを確認する。
   * @param id チャンクの BufferViewHandle
   * @return 有効な場合 true
   */
  bool isAlive(BufferViewHandle handle) const {

    const ChunkInfo *chunk = find(handle);
    return chunk && chunk->alive;
  }

  // ========================================================================
  // ID encoding/decoding
  // ========================================================================

  BufferViewHandle encodeId(std::size_t slot) const {
    return BufferViewHandle{
        static_cast<BufferViewHandle::underlying_type>(slot) & kChunkMask};
  }

  std::size_t indexFromId(BufferViewHandle handle) const {
    return static_cast<std::size_t>(
        static_cast<BufferViewHandle::underlying_type>(handle) & kChunkMask);
  }

private:
  // ========================================================================
  // Internal types
  // ========================================================================
  struct ChunkInfo {
    BufferView base{};
    std::size_t size{};
    std::size_t alignment{};
    uint32_t used{};
    uint32_t pending{};
    bool alive{false};
  };

  // ========================================================================
  // Constants
  // ========================================================================
  static constexpr BufferViewHandle::underlying_type kLargeMask =
      BufferViewHandle::underlying_type{1u} << 31;
  static constexpr BufferViewHandle::underlying_type kChunkMask = ~kLargeMask;

  // ========================================================================
  // Internal methods
  // ========================================================================
  ChunkInfo *find(BufferViewHandle handle) {
    const std::size_t slot = indexFromId(handle);
    if (slot >= chunks_.size()) {
      return nullptr;
    }
    ChunkInfo &chunk = chunks_[slot];
    return chunk.alive ? &chunk : nullptr;
  }

  const ChunkInfo *find(BufferViewHandle handle) const {
    const std::size_t slot = indexFromId(handle);
    if (slot >= chunks_.size()) {
      return nullptr;
    }
    const ChunkInfo &chunk = chunks_[slot];
    return chunk.alive ? &chunk : nullptr;
  }

  std::size_t reserveSlot() {
    if (!free_list_.empty()) {
      const auto slot = free_list_.back();
      free_list_.resize(free_list_.size() - 1);
      return slot;
    }
    chunks_.emplaceBack();
    return chunks_.size() - 1;
  }

  // ========================================================================
  // Member variables
  // ========================================================================
  Config config_{};
  Resource *resource_{nullptr};

  ::orteaf::internal::base::HeapVector<ChunkInfo> chunks_;
  ::orteaf::internal::base::HeapVector<std::size_t> free_list_;
};

} // namespace orteaf::internal::execution::allocator::policies

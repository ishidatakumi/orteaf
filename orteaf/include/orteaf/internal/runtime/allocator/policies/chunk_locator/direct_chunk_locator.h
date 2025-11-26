#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/backend/backend_traits.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/base/strong_id.h"
#include "orteaf/internal/runtime/allocator/memory_block.h"
#include "orteaf/internal/runtime/allocator/policies/chunk_locator/chunk_locator_config.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/diagnostics/error/error_macros.h"

namespace orteaf::internal::runtime::allocator::policies {

/**
 * @brief Direct スタイルの ChunkLocator ポリシー。
 *
 * BufferId の上位ビットで large/chunk を判別し、下位ビットをチャンクのスロットに割り当てる。
 * device/context ごとに分けず、配列ひとつで O(1) アクセスにする。
 *
 * @tparam Resource リソース管理クラス
 * @tparam B バックエンド種別
 */
template <typename Resource, ::orteaf::internal::backend::Backend B>
class DirectChunkLocatorPolicy {
public:
    // ========================================================================
    // Type aliases
    // ========================================================================
    using BufferId = ::orteaf::internal::base::BufferId;
    using BufferView = typename ::orteaf::internal::backend::BackendTraits<B>::BufferView;
    using MemoryBlock = ::orteaf::internal::runtime::allocator::MemoryBlock<B>;
    using Device = typename ::orteaf::internal::backend::BackendTraits<B>::Device;
    using Context = typename ::orteaf::internal::backend::BackendTraits<B>::Context;
    using Stream = typename ::orteaf::internal::backend::BackendTraits<B>::Stream;

    /**
     * @brief DirectChunkLocatorPolicy 固有の設定。
     */
    struct Config : ChunkLocatorConfigBase<Device, Context, Stream> {
        // 現時点では追加設定なし（将来の拡張用）
    };

    // ========================================================================
    // Public API
    // ========================================================================

    /**
     * @brief ポリシーを初期化する。
     * @param config 設定
     * @param resource リソース管理オブジェクト（非所有）
     */
    void initialize(const Config& config, Resource* resource) {
        ORTEAF_THROW_IF_NULL(resource, "DirectChunkLocatorPolicy requires non-null Resource*");
        config_ = config;
        resource_ = resource;
    }

    /**
     * @brief チャンクを確保して登録し、対応する MemoryBlock を返す。
     * @param size 確保サイズ
     * @param alignment アラインメント
     * @return 確保された MemoryBlock（失敗時は空）
     */
    MemoryBlock addChunk(std::size_t size, std::size_t alignment) {
        std::lock_guard<std::mutex> lock(mutex_);
        ORTEAF_THROW_IF(resource_ == nullptr, InvalidState, "DirectChunkLocatorPolicy is not initialized");
        ORTEAF_THROW_IF(size == 0, InvalidParameter, "size must be non-zero");

        BufferView base = resource_->allocate(size, alignment, config_.stream);
        if (!base) {
            return {};
        }

        const std::size_t slot = reserveSlot();
        chunks_[slot] = ChunkInfo{base, size, alignment, 0u, 0u, true};
        return MemoryBlock{encodeId(slot), base};
    }

    /**
     * @brief チャンク全体を解放する（used/pending が 0 のときのみ）。
     * @param id 解放するチャンクの BufferId
     * @return 解放に成功した場合 true
     */
    bool releaseChunk(BufferId id) {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::size_t slot = indexFromId(id);
        if (slot >= chunks_.size() || resource_ == nullptr) {
            return false;
        }

        ChunkInfo& chunk = chunks_[slot];
        if (!chunk.alive || chunk.used != 0 || chunk.pending != 0) {
            return false;
        }

        resource_->deallocate(chunk.base, chunk.size, chunk.alignment, config_.stream);
        chunk = ChunkInfo{};
        free_list_.pushBack(slot);
        return true;
    }

    /**
     * @brief チャンクサイズを取得する。
     * @param id チャンクの BufferId
     * @return チャンクサイズ（無効な場合 0）
     */
    std::size_t findChunkSize(BufferId id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        const ChunkInfo* chunk = find(id);
        return chunk ? chunk->size : 0;
    }

    void incrementUsed(BufferId id) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (auto* chunk = find(id)) {
            ++chunk->used;
        }
    }

    void decrementUsed(BufferId id) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (auto* chunk = find(id)) {
            if (chunk->used > 0) {
                --chunk->used;
            }
        }
    }

    void incrementPending(BufferId id) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (auto* chunk = find(id)) {
            ++chunk->pending;
        }
    }

    void decrementPending(BufferId id) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (auto* chunk = find(id)) {
            if (chunk->pending > 0) {
                --chunk->pending;
            }
        }
    }

    void decrementPendingAndUsed(BufferId id) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (auto* chunk = find(id)) {
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
     * @param id チャンクの BufferId
     * @return 有効な場合 true
     */
    bool isAlive(BufferId id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        const ChunkInfo* chunk = find(id);
        return chunk && chunk->alive;
    }

    // ========================================================================
    // ID encoding/decoding
    // ========================================================================

    BufferId encodeId(std::size_t slot) const {
        return BufferId{static_cast<BufferId::underlying_type>(slot) & kChunkMask};
    }

    std::size_t indexFromId(BufferId id) const {
        return static_cast<std::size_t>(static_cast<BufferId::underlying_type>(id) & kChunkMask);
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
    static constexpr BufferId::underlying_type kLargeMask = BufferId::underlying_type{1u} << 31;
    static constexpr BufferId::underlying_type kChunkMask = ~kLargeMask;

    // ========================================================================
    // Internal methods
    // ========================================================================
    ChunkInfo* find(BufferId id) {
        const std::size_t slot = indexFromId(id);
        if (slot >= chunks_.size()) {
            return nullptr;
        }
        ChunkInfo& chunk = chunks_[slot];
        return chunk.alive ? &chunk : nullptr;
    }

    const ChunkInfo* find(BufferId id) const {
        const std::size_t slot = indexFromId(id);
        if (slot >= chunks_.size()) {
            return nullptr;
        }
        const ChunkInfo& chunk = chunks_[slot];
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
    Resource* resource_{nullptr};
    mutable std::mutex mutex_;
    base::HeapVector<ChunkInfo> chunks_;
    base::HeapVector<std::size_t> free_list_;
};

}  // namespace orteaf::internal::runtime::allocator::policies

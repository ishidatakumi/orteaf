#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/backend/backend_traits.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/base/strong_id.h"
#include "orteaf/internal/runtime/allocator/memory_block.h"

namespace orteaf::internal::runtime::allocator::policies {

// Chunk locator that mirrors the direct_resource style: BufferId の上位ビットで
// large/chunk を判別し、下位ビットを chunk のスロットに割り当てる。
// device/context ごとに分けず、配列ひとつで O(1) アクセスにする。
template <typename Resource, ::orteaf::internal::backend::Backend B>
class DirectChunkLocatorPolicy {
public:
    using BufferId = ::orteaf::internal::base::BufferId;
    using BufferView = typename ::orteaf::internal::backend::BackendTraits<B>::BufferView;
    using MemoryBlock = ::orteaf::internal::runtime::allocator::MemoryBlock<B>;
    using Device = typename ::orteaf::internal::backend::BackendTraits<B>::Device;
    using Context = typename ::orteaf::internal::backend::BackendTraits<B>::Context;
    using Stream = typename ::orteaf::internal::backend::BackendTraits<B>::Stream;

    void initialize(Device device, Context context, Stream stream) {
        device_ = device;
        context_ = context;
        stream_ = stream;
    }

    // チャンクを登録し、BufferId を返す（上位ビットは large 判定用に 0 のまま）。
    BufferId addChunk(BufferView base, std::size_t size, std::size_t block_size) {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::size_t slot = reserveSlot();
        chunks_[slot] = ChunkInfo{base, size, block_size, 0u, 0u, true};
        return encodeId(slot);
    }

    // チャンク配下のブロックに紐づく MemoryBlock を作る際のヘルパー。
    MemoryBlock makeBlock(BufferId id, BufferView view) const {
        return MemoryBlock{id, view};
    }

    void incrementUsed(BufferId id) {
        if (auto* chunk = find(id)) {
            ++chunk->used;
        }
    }

    void decrementUsed(BufferId id) {
        if (auto* chunk = find(id)) {
            if (chunk->used > 0) {
                --chunk->used;
            }
        }
    }

    void incrementPending(BufferId id) {
        if (auto* chunk = find(id)) {
            ++chunk->pending;
        }
    }

    void decrementPending(BufferId id) {
        if (auto* chunk = find(id)) {
            if (chunk->pending > 0) {
                --chunk->pending;
            }
        }
    }

    void decrementPendingAndUsed(BufferId id) {
        if (auto* chunk = find(id)) {
            if (chunk->pending > 0) {
                --chunk->pending;
            }
            if (chunk->used > 0) {
                --chunk->used;
            }
        }
    }

    std::size_t findBlockSize(BufferId id) const {
        const ChunkInfo* chunk = find(id);
        return chunk ? chunk->block_size : 0;
    }

    // チャンク全体を解放する（used/pending が 0 のときのみ）。
    bool releaseChunk(BufferId id, std::size_t alignment = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::size_t slot = indexFromId(id);
        if (slot >= chunks_.size()) {
            return false;
        }
        ChunkInfo& chunk = chunks_[slot];
        if (!chunk.alive || chunk.used != 0 || chunk.pending != 0) {
            return false;
        }

        Resource::deallocate(chunk.base, chunk.size, alignment, device_, stream_);
        chunk = ChunkInfo{};
        free_list_.pushBack(slot);
        return true;
    }

    bool isAlive(BufferId id) const {
        const ChunkInfo* chunk = find(id);
        return chunk && chunk->alive;
    }

    BufferId encodeId(std::size_t slot) const {
        return BufferId{static_cast<BufferId::underlying_type>(slot) & kChunkMask};
    }

    std::size_t indexFromId(BufferId id) const {
        return static_cast<std::size_t>(static_cast<BufferId::underlying_type>(id) & kChunkMask);
    }

private:
    struct ChunkInfo {
        BufferView base{};
        std::size_t size{};
        std::size_t block_size{};
        uint32_t used{};
        uint32_t pending{};
        bool alive{false};
    };

    static constexpr BufferId::underlying_type kLargeMask = BufferId::underlying_type{1u} << 31;
    static constexpr BufferId::underlying_type kChunkMask = ~kLargeMask;

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

    mutable std::mutex mutex_;
    base::HeapVector<ChunkInfo> chunks_;
    base::HeapVector<std::size_t> free_list_;

    Device device_{};
    Context context_{};
    Stream stream_{};
};

}  // namespace orteaf::internal::runtime::allocator::policies

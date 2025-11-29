#pragma once

#include <orteaf/internal/backend/backend.h>
#include <orteaf/internal/backend/backend_traits.h>
#include <orteaf/internal/runtime/allocator/memory_block.h>

namespace orteaf::internal::runtime::allocator::pool {
template <
    typename BackendResource,
    typename FastFreePolicy,
    typename ThreadingPolicy,
    typename LargeAllocPolicy,
    typename ChunkLocatorPolicy,
    typename ReuseLocatorPolicy,
    typename FreeListPolicy,
    ::orteaf::internal::backend::Backend BackendType>
class SegregatePool {
public:
    using MemoryBlock = typename BackendResource::MemoryBlock;

    SegregatePool(const SegregatePool&) = delete;
    SegregatePool& operator=(const SegregatePool&) = delete;
    SegregatePool(SegregatePool&&) noexcept = default;
    SegregatePool& operator=(SegregatePool&&) noexcept = default;

    struct Config {
        typename FastFreePolicy::template Config<BackendResource> fast_free{};
        typename ThreadingPolicy::template Config<BackendResource> threading{};
        typename LargeAllocPolicy::Config large_alloc{};
        typename ChunkLocatorPolicy::Config chunk_locator{};
        typename ReuseLocatorPolicy::Config reuse{};
        typename FreeListPolicy::Config freelist{};

        std::size_t chunk_size{16 * 1024 * 1024};
        std::size_t min_block_size{64};
        std::size_t max_block_size{0};
    };

    void initialize(const Config& config) {
        fast_free_policy_.initialize(config.fast_free);
        threading_policy_.initialize(config.threading);
        large_alloc_policy_.initialize(config.large_alloc);
        chunk_locator_policy_.initialize(config.chunk_locator);
        reuse_policy_.initialize(config.reuse);
        free_list_policy_.initialize(config.freelist);

        chunk_size_ = config.chunk_size;
        min_block_size_ = config.min_block_size;
        max_block_size_ = config.max_block_size;
    }

    FastFreePolicy& fast_free_policy() { return fast_free_policy_; }
    ThreadingPolicy& threading_policy() { return threading_policy_; }
    LargeAllocPolicy& large_alloc_policy() { return large_alloc_policy_; }
    ChunkLocatorPolicy& chunk_locator_policy() { return chunk_locator_policy_; }
    ReuseLocatorPolicy& reuse_policy() { return reuse_policy_; }
    FreeListPolicy& free_list_policy() { return free_list_policy_; }

    MemoryBlock allocate(std::size_t size, std::size_t alignment) {
        
    }

    void deallocate(const MemoryBlock& block, std::size_t size, std::size_t alignment);

    void processPendingReuses() {
        reuse_policy_.processPending();

        MemoryBlock block{};
        std::size_t freelist_index = 0;

        while (reuse_policy_.getReadyItem(block, freelist_index)) {
            free_list_policy_.push(block, freelist_index);
        }
    }

    void releaseChunk();

private:


    void expandPool(std::size_t list_idx, std::size_t block_size) {
        const std::size_t num_blocks = (chunk_size_ + block_size - 1) / block_size;
        const std::size_t actural_chunk_size = num_blocks * block_size;

        MemoryBlock chunk = chunk_locator_policy_.addChunk(actural_chunk_size, 0);
        if (!chunk.valid()) return;

        free_list_policy_.expand(list_idx, chunk, actural_chunk_size, block_size);
    }

    std::size_t min_block_size_{64};
    std::size_t max_block_size_{0};

    std::size_t chunk_size_{0};

    BackendResource backend_resource_;
    FastFreePolicy fast_free_policy_;
    ThreadingPolicy threading_policy_;
    LargeAllocPolicy large_alloc_policy_;
    ChunkLocatorPolicy chunk_locator_policy_;
    ReuseLocatorPolicy reuse_policy_;
    FreeListPolicy free_list_policy_;
    BackendType backend_type_;
};

}  // namespace orteaf::internal::runtime::allocator::pool

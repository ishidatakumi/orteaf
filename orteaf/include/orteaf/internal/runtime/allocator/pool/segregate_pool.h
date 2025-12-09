#pragma once

#include <algorithm>
#include <bit>
#include <limits>
#include <orteaf/internal/backend/backend.h>
#include <orteaf/internal/runtime/allocator/memory_block.h>
#include <orteaf/internal/runtime/allocator/pool/segregate_pool_stats.h>
#include <orteaf/internal/runtime/base/backend_traits.h>

namespace orteaf::internal::runtime::allocator::pool {
template <typename BackendResource, typename FastFreePolicy,
          typename ThreadingPolicy, typename LargeAllocPolicy,
          typename ChunkLocatorPolicy, typename ReuseLocatorPolicy,
          typename FreeListPolicy,
          ::orteaf::internal::backend::Backend BackendType>
class SegregatePool {
public:
  using BufferResource = typename BackendResource::BufferResource;
  using LaunchParams =
      typename ::orteaf::internal::runtime::base::BackendTraits<
          BackendType>::KernelLaunchParams;

  SegregatePool() = default;
  SegregatePool(const SegregatePool &) = delete;
  SegregatePool &operator=(const SegregatePool &) = delete;
  SegregatePool(SegregatePool &&) noexcept = default;
  SegregatePool &operator=(SegregatePool &&) noexcept = default;

  struct Config {
    typename FastFreePolicy::template Config<BackendResource> fast_free{};
    typename ThreadingPolicy::template Config<BackendResource> threading{};
    typename LargeAllocPolicy::Config large_alloc{};
    typename ChunkLocatorPolicy::Config chunk_locator{};
    typename ReuseLocatorPolicy::Config reuse{};
    typename FreeListPolicy::Config freelist{};

    std::size_t chunk_size{16 * 1024 * 1024};
    std::size_t min_block_size{64};
    std::size_t max_block_size{std::numeric_limits<std::size_t>::max()};
  };

  void initialize(const Config &config) {
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

  FastFreePolicy &fast_free_policy() { return fast_free_policy_; }
  ThreadingPolicy &threading_policy() { return threading_policy_; }
  LargeAllocPolicy &large_alloc_policy() { return large_alloc_policy_; }
  ChunkLocatorPolicy &chunk_locator_policy() { return chunk_locator_policy_; }
  ReuseLocatorPolicy &reuse_policy() { return reuse_policy_; }
  FreeListPolicy &free_list_policy() { return free_list_policy_; }

  const SegregatePoolStats<BackendType> &stats() const { return stats_; }

  BufferResource allocate(std::size_t size, std::size_t alignment,
                       LaunchParams &launch_params) {
    if (size == 0)
      return BufferResource{};

    std::lock_guard<ThreadingPolicy> lock(threading_policy_);

    if (size > max_block_size_) {
      stats_.updateAlloc(size, true);
      return large_alloc_policy_.allocate(size, alignment);
    }

    processPendingReuses(launch_params);

    const std::size_t block_size =
        std::bit_ceil(std::max(min_block_size_, size));
    const std::size_t list_idx =
        std::countr_zero(std::bit_ceil(block_size)) -
        std::countr_zero(std::bit_ceil(min_block_size_));

    BufferResource block = free_list_policy_.pop(list_idx, launch_params);

    if (!block.valid()) {
      expandPool(list_idx, block_size, launch_params);
      block = free_list_policy_.pop(list_idx, launch_params);
      if (!block.valid()) {
        return {};
      }
    }

    chunk_locator_policy_.incrementUsed(block.handle);

    stats_.updateAlloc(size, false);
    return block;
  }

  void deallocate(const BufferResource &block, std::size_t size,
                  std::size_t alignment, LaunchParams &launch_params) {
    if (!block.valid() || size == 0)
      return;

    std::lock_guard<ThreadingPolicy> lock(threading_policy_);

    if (size > max_block_size_) {
      large_alloc_policy_.deallocate(block.handle, size, alignment);
      stats_.updateDealloc(size);
      return;
    }

    const std::size_t block_size =
        fast_free_policy_.get_block_size(min_block_size_, size);
    const std::size_t list_idx =
        std::countr_zero(std::bit_ceil(block_size)) -
        std::countr_zero(std::bit_ceil(min_block_size_));

    chunk_locator_policy_.incrementPending(block.handle);
    reuse_policy_.scheduleForReuse(block, list_idx, {});
    stats_.updateDealloc(size);
  }

  void processPendingReuses(LaunchParams &launch_params) {
    reuse_policy_.processPending();

    std::size_t freelist_index = 0;
    BufferResource block{};

    while (reuse_policy_.getReadyItem(freelist_index, block)) {
      chunk_locator_policy_.decrementPendingAndUsed(block.handle);
      free_list_policy_.push(freelist_index, block, launch_params);
    }
  }

  void releaseChunk(LaunchParams &launch_params) {
    std::lock_guard<ThreadingPolicy> lock(threading_policy_);

    processPendingReuses(launch_params);

    while (true) {
      const auto handle = chunk_locator_policy_.findReleasable();
      if (!handle.isValid())
        break;

      reuse_policy_.removeBlocksInChunk(handle);
      free_list_policy_.removeBlocksInChunk(handle);

      if (!chunk_locator_policy_.releaseChunk(handle)) {
        break;
      }
    }
  }

private:
  void expandPool(std::size_t list_idx, std::size_t block_size,
                  LaunchParams &launch_params) {
    const std::size_t num_blocks = (chunk_size_ + block_size - 1) / block_size;
    const std::size_t actual_chunk_size = num_blocks * block_size;

    BufferResource chunk = chunk_locator_policy_.addChunk(actual_chunk_size, 0);
    if (!chunk.valid())
      return;

    free_list_policy_.expand(list_idx, chunk, actual_chunk_size, block_size,
                             launch_params);
    stats_.updateExpansion();
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
  ::orteaf::internal::backend::Backend backend_type_{BackendType};
  SegregatePoolStats<BackendType> stats_;
};

} // namespace orteaf::internal::runtime::allocator::pool

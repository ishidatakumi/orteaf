#pragma once

#include <algorithm>
#include <limits>
#include <orteaf/internal/backend/backend.h>
#include <orteaf/internal/execution/allocator/buffer_resource.h>
#include <orteaf/internal/execution/allocator/pool/segregate_pool_stats.h>
#include <orteaf/internal/execution/allocator/size_class_utils.h>

namespace orteaf::internal::execution::allocator::pool {
template <typename BackendResource, typename FastFreePolicy,
          typename ThreadingPolicy, typename LargeAllocPolicy,
          typename ChunkLocatorPolicy, typename ReuseLocatorPolicy,
          typename FreeListPolicy>
class SegregatePool {
public:
  static constexpr auto BackendType = BackendResource::backend_type_static();
  using BufferResource = typename BackendResource::BufferResource;
  using BufferBlock =
      ::orteaf::internal::execution::allocator::BufferBlock<BackendType>;
  using LaunchParams = typename BackendResource::LaunchParams;
  using Stats = SegregatePoolStats<BackendType>;

  SegregatePool() = default;
  explicit SegregatePool(BackendResource resource)
      : resource_(std::move(resource)) {}
  SegregatePool(const SegregatePool &) = delete;
  SegregatePool &operator=(const SegregatePool &) = delete;

  SegregatePool(SegregatePool &&other) noexcept
      : min_block_size_(other.min_block_size_),
        max_block_size_(other.max_block_size_), chunk_size_(other.chunk_size_),
        resource_(std::move(other.resource_)),
        fast_free_policy_(std::move(other.fast_free_policy_)),
        threading_policy_(std::move(other.threading_policy_)),
        large_alloc_policy_(std::move(other.large_alloc_policy_)),
        chunk_locator_policy_(std::move(other.chunk_locator_policy_)),
        reuse_policy_(std::move(other.reuse_policy_)),
        free_list_policy_(std::move(other.free_list_policy_)),
        stats_(std::move(other.stats_)) {}

  SegregatePool &operator=(SegregatePool &&other) noexcept {
    if (this != &other) {
      min_block_size_ = other.min_block_size_;
      max_block_size_ = other.max_block_size_;
      chunk_size_ = other.chunk_size_;
      resource_ = std::move(other.resource_);
      fast_free_policy_ = std::move(other.fast_free_policy_);
      threading_policy_ = std::move(other.threading_policy_);
      large_alloc_policy_ = std::move(other.large_alloc_policy_);
      chunk_locator_policy_ = std::move(other.chunk_locator_policy_);
      reuse_policy_ = std::move(other.reuse_policy_);
      free_list_policy_ = std::move(other.free_list_policy_);
      stats_ = std::move(other.stats_);
    }
    return *this;
  }

  ~SegregatePool() = default;

  struct Config {
    typename FastFreePolicy::template Config<BackendResource> fast_free{};
    typename ThreadingPolicy::template Config<BackendResource> threading{};
    typename LargeAllocPolicy::Config large_alloc{};
    typename ChunkLocatorPolicy::Config chunk_locator{};
    typename ReuseLocatorPolicy::Config reuse{};
    typename FreeListPolicy::Config freelist{};

    std::size_t chunk_size{16 * 1024 * 1024};
    std::size_t min_block_size{64};
    std::size_t max_block_size{16 * 1024 * 1024}; // デフォルトを適切な値に
  };

  void initialize(const Config &config) {
    chunk_size_ = config.chunk_size;
    min_block_size_ = config.min_block_size;
    max_block_size_ = config.max_block_size;

    // サイズクラス数を計算（SegregatePool が一元管理）
    const std::size_t size_class_count =
        sizeClassCount(min_block_size_, max_block_size_);

    fast_free_policy_.initialize(config.fast_free);
    threading_policy_.initialize(config.threading);
    large_alloc_policy_.initialize(config.large_alloc);
    chunk_locator_policy_.initialize(config.chunk_locator);
    reuse_policy_.initialize(config.reuse);
    // freelist にサイズクラス数を渡す
    free_list_policy_.initialize(config.freelist, size_class_count);
  }

  FastFreePolicy &fast_free_policy() { return fast_free_policy_; }
  ThreadingPolicy &threading_policy() { return threading_policy_; }
  LargeAllocPolicy &large_alloc_policy() { return large_alloc_policy_; }
  ChunkLocatorPolicy &chunk_locator_policy() { return chunk_locator_policy_; }
  ReuseLocatorPolicy &reuse_policy() { return reuse_policy_; }
  FreeListPolicy &free_list_policy() { return free_list_policy_; }

  BackendResource *resource() { return &resource_; }
  const BackendResource *resource() const { return &resource_; }

  std::size_t min_block_size() const { return min_block_size_; }
  std::size_t max_block_size() const { return max_block_size_; }

  const Stats &stats() const { return stats_; }

  BufferResource allocate(std::size_t size, std::size_t alignment,
                          LaunchParams &launch_params) {
    if (size == 0)
      return BufferResource{};

    std::lock_guard<ThreadingPolicy> lock(threading_policy_);

    if (size > max_block_size_) {
      stats_.updateAlloc(size, true);
      BufferBlock block = large_alloc_policy_.allocate(size, alignment);
      return BufferResource::fromBlock(block);
    }

    processPendingReuses(launch_params);

    const std::size_t block_size = blockSizeFor(size);
    const std::size_t list_idx = sizeClassIndex(block_size, min_block_size_);

    BufferBlock block = free_list_policy_.pop(list_idx, launch_params);

    if (!block.valid()) {
      expandPool(list_idx, block_size, launch_params);
      block = free_list_policy_.pop(list_idx, launch_params);
      if (!block.valid()) {
        return {};
      }
    }

    chunk_locator_policy_.incrementUsed(block.handle);

    stats_.updateAlloc(size, false);
    return BufferResource::fromBlock(block);
  }

  void deallocate(BufferResource block, std::size_t size, std::size_t alignment,
                  LaunchParams &launch_params) {
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
    const std::size_t list_idx = sizeClassIndex(block_size, min_block_size_);

    chunk_locator_policy_.incrementPending(block.handle);
    reuse_policy_.scheduleForReuse(std::move(block), list_idx);
    stats_.updateDealloc(size);
  }

  void processPendingReuses(LaunchParams &launch_params) {
    reuse_policy_.processPending();

    std::size_t freelist_index = 0;
    BufferBlock ready_block{};

    while (reuse_policy_.getReadyItem(freelist_index, ready_block)) {
      chunk_locator_policy_.decrementPendingAndUsed(ready_block.handle);
      free_list_policy_.push(freelist_index, ready_block, launch_params);
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
  /**
   * @brief サイズに対応するブロックサイズを計算
   */
  std::size_t blockSizeFor(std::size_t size) const {
    return sizeClassToBlockSize(
        sizeClassIndex(std::max(min_block_size_, size), min_block_size_),
        min_block_size_);
  }

  void expandPool(std::size_t list_idx, std::size_t block_size,
                  LaunchParams &launch_params) {
    const std::size_t num_blocks = (chunk_size_ + block_size - 1) / block_size;
    const std::size_t actual_chunk_size = num_blocks * block_size;

    BufferBlock chunk = chunk_locator_policy_.addChunk(actual_chunk_size, 0);
    if (!chunk.valid())
      return;

    free_list_policy_.expand(list_idx, chunk, actual_chunk_size, block_size,
                             launch_params);
    stats_.updateExpansion();
  }

  std::size_t min_block_size_{64};
  std::size_t max_block_size_{0};

  std::size_t chunk_size_{0};

  BackendResource resource_;
  FastFreePolicy fast_free_policy_;
  ThreadingPolicy threading_policy_;
  LargeAllocPolicy large_alloc_policy_;
  ChunkLocatorPolicy chunk_locator_policy_;
  ReuseLocatorPolicy reuse_policy_;
  FreeListPolicy free_list_policy_;
  Stats stats_;
};

} // namespace orteaf::internal::execution::allocator::pool

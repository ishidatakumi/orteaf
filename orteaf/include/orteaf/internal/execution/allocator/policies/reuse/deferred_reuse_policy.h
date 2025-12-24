#pragma once

#include <chrono>
#include <cstddef>
#include <thread>
#include <utility>

#include <orteaf/internal/backend/backend.h>
#include <orteaf/internal/base/handle.h>
#include <orteaf/internal/base/heap_vector.h>
#include <orteaf/internal/diagnostics/error/error_macros.h>
#include <orteaf/internal/execution/allocator/buffer_resource.h>
#include <orteaf/internal/execution/allocator/policies/policy_config.h>

namespace orteaf::internal::execution::allocator::policies {

template <typename Resource> class DeferredReusePolicy {
public:
  static constexpr auto kBackend = Resource::backend_type_static();
  using BufferResource = typename Resource::BufferResource;
  using BufferBlock =
      ::orteaf::internal::execution::allocator::BufferBlock<kBackend>;
  using BufferView = typename BufferResource::BufferView;
  using BufferViewHandle = typename BufferResource::BufferViewHandle;
  using ReuseToken = typename Resource::ReuseToken;
  using FenceToken = typename BufferResource::FenceToken;

  DeferredReusePolicy() = default;
  DeferredReusePolicy(const DeferredReusePolicy &) = delete;
  DeferredReusePolicy &operator=(const DeferredReusePolicy &) = delete;
  DeferredReusePolicy(DeferredReusePolicy &&) = default;
  DeferredReusePolicy &operator=(DeferredReusePolicy &&) = default;
  ~DeferredReusePolicy() = default;

  struct Config : PolicyConfig<Resource> {
    std::chrono::milliseconds timeout_ms{std::chrono::milliseconds{1000}};
  };

  void initialize(const Config &config = {}) {
    ORTEAF_THROW_IF_NULL(config.resource,
                         "DeferredReusePolicy requires non-null Resource*");
    resource_ = config.resource;
    timeout_ms_ = config.timeout_ms;
  }

  void scheduleForReuse(BufferResource block, std::size_t freelist_index) {
    ORTEAF_THROW_IF(resource_ == nullptr, InvalidState,
                    "DeferredReusePolicy is not initialized");
    // Extract BufferBlock and convert FenceToken to ReuseToken
    PendingReuse pending{BufferBlock{block.handle, std::move(block.view)},
                         ReuseToken(std::move(block.fence_token)),
                         freelist_index, std::chrono::steady_clock::now()};
    pending_queue_.pushBack(std::move(pending));
  }

  std::size_t processPending() {
    ORTEAF_THROW_IF(resource_ == nullptr, InvalidState,
                    "DeferredReusePolicy is not initialized");

    const auto now = std::chrono::steady_clock::now();
    std::size_t ready_count = 0;
    std::size_t write_idx = 0;

    for (std::size_t i = 0; i < pending_queue_.size(); ++i) {
      PendingReuse &item = pending_queue_[i];
      const bool timed_out = (now - item.timestamp) > timeout_ms_;
      const bool completed = resource_->isCompleted(item.reuse_token);

      if (completed) {
        ready_queue_.emplaceBack(
            ReadyReuse{std::move(item.block), item.freelist_index});
        ++ready_count;
      } else {
        // タイムアウトしても未完了の場合、早期再利用を避けるため残す。
        if (timed_out) {
          item.timestamp = now; // 同じtimeout判定での連続ヒットを避ける
        }
        if (write_idx != i) {
          pending_queue_[write_idx] = std::move(item);
        }
        ++write_idx;
      }
    }

    if (write_idx < pending_queue_.size()) {
      pending_queue_.resize(write_idx);
    }

    return ready_count;
  }

  bool hasPending() const { return !pending_queue_.empty(); }

  std::size_t getPendingReuseCount() const {
    return pending_queue_.size() + ready_queue_.size();
  }

  void flushPending() {
    while (!pending_queue_.empty()) {
      const auto processed = processPending();
      if (processed == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }
  }

  bool getReadyItem(std::size_t &freelist_index, BufferBlock &result) {
    if (ready_queue_.empty())
      return false;
    ReadyReuse item = std::move(ready_queue_.back());
    ready_queue_.resize(ready_queue_.size() - 1);

    result = std::move(item.block);
    freelist_index = item.freelist_index;
    return true;
  }

  void removeBlocksInChunk(const BufferViewHandle &chunk_handle) {
    filterPending(chunk_handle);
    filterReady(chunk_handle);
  }

  void setTimeout(std::chrono::milliseconds timeout_ms) {
    timeout_ms_ = timeout_ms;
  }

private:
  struct PendingReuse {
    BufferBlock block;
    ReuseToken reuse_token;
    std::size_t freelist_index;
    std::chrono::steady_clock::time_point timestamp;
  };

  struct ReadyReuse {
    BufferBlock block;
    std::size_t freelist_index;
  };

  void filterPending(const BufferViewHandle &chunk_handle) {
    ::orteaf::internal::base::HeapVector<PendingReuse> filtered;
    for (std::size_t i = 0; i < pending_queue_.size(); ++i) {
      if (pending_queue_[i].block.handle != chunk_handle) {
        filtered.pushBack(std::move(pending_queue_[i]));
      }
    }
    pending_queue_ = std::move(filtered);
  }

  void filterReady(const BufferViewHandle &chunk_handle) {
    ::orteaf::internal::base::HeapVector<ReadyReuse> filtered;
    for (std::size_t i = 0; i < ready_queue_.size(); ++i) {
      if (ready_queue_[i].block.handle != chunk_handle) {
        filtered.pushBack(std::move(ready_queue_[i]));
      }
    }
    ready_queue_ = std::move(filtered);
  }

  ::orteaf::internal::base::HeapVector<PendingReuse> pending_queue_{};
  ::orteaf::internal::base::HeapVector<ReadyReuse> ready_queue_{};
  std::chrono::milliseconds timeout_ms_{std::chrono::milliseconds{0}};
  Resource *resource_{nullptr};
};

} // namespace orteaf::internal::execution::allocator::policies

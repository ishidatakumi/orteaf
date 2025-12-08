#pragma once

#include <bit>
#include <cstddef>
#include <utility>

#include <orteaf/internal/backend/backend.h>
#include <orteaf/internal/base/heap_vector.h>
#include <orteaf/internal/diagnostics/error/error_macros.h>
#include <orteaf/internal/runtime/allocator/memory_block.h>
#include <orteaf/internal/runtime/base/backend_traits.h>

namespace orteaf::internal::runtime::allocator::policies {

/**
 * @brief Host 管理のスタック型フリーリストポリシー。
 *
 * サイズクラス別のスタックをひとつだけ持ち、Host 側で割り当て・解放を管理する。
 * デバイスやストリーム情報は上位レイヤーで管理している前提で、本ポリシーでは扱わない。
 */
template <typename Resource, ::orteaf::internal::backend::Backend B>
class HostStackFreelistPolicy {
public:
  using MemoryBlock = ::orteaf::internal::runtime::allocator::MemoryBlock<B>;

  void initialize(Resource *resource) {
    ORTEAF_THROW_IF_NULL(resource,
                         "HostStackFreelistPolicy requires non-null Resource*");
    resource_ = resource;
  }

  void configureBounds(std::size_t min_block_size, std::size_t max_block_size) {
    ORTEAF_THROW_IF(resource_ == nullptr, InvalidState,
                    "HostStackFreelistPolicy is not initialized");
    min_block_size_ = min_block_size;
    size_class_count_ = std::countr_zero(std::bit_ceil(max_block_size)) -
                        std::countr_zero(std::bit_ceil(min_block_size)) + 1;
    stacks_.resize(size_class_count_);
  }

  void push(std::size_t list_index, const MemoryBlock &block) {
    ORTEAF_THROW_IF(resource_ == nullptr, InvalidState,
                    "HostStackFreelistPolicy is not initialized");
    if (list_index >= stacks_.size()) {
      stacks_.resize(std::max(stacks_.size(), list_index + 1));
    }
    stacks_[list_index].pushBack(block);
  }

  MemoryBlock pop(std::size_t list_index) {
    ORTEAF_THROW_IF(resource_ == nullptr, InvalidState,
                    "HostStackFreelistPolicy is not initialized");
    if (list_index >= stacks_.size() || stacks_[list_index].empty()) {
      return {};
    }
    MemoryBlock block = std::move(stacks_[list_index].back());
    stacks_[list_index].resize(stacks_[list_index].size() - 1);
    return block;
  }

  bool empty(std::size_t list_index) const {
    return list_index >= stacks_.size() || stacks_[list_index].empty();
  }

  std::size_t get_active_freelist_count() const {
    return stacks_.empty() ? 0 : 1;
  }

  std::size_t get_total_free_blocks() const {
    std::size_t total = 0;
    for (const auto &stack : stacks_) {
      total += stack.size();
    }
    return total;
  }

  void expand(std::size_t list_index, const MemoryBlock &chunk,
              std::size_t chunk_size, std::size_t block_size) {
    ORTEAF_THROW_IF(resource_ == nullptr, InvalidState,
                    "HostStackFreelistPolicy is not initialized");
    if (!chunk.valid() || block_size == 0) {
      return;
    }

    if (list_index >= stacks_.size()) {
      stacks_.resize(std::max(stacks_.size(), list_index + 1));
    }

    const std::size_t num_blocks = chunk_size / block_size;
    const std::size_t base_offset = chunk.view.offset();
    for (std::size_t i = 0; i < num_blocks; ++i) {
      const std::size_t offset = base_offset + i * block_size;
      MemoryBlock block{chunk.handle,
                        Resource::makeView(chunk.view, offset, block_size)};
      stacks_[list_index].pushBack(std::move(block));
    }
  }

  void removeBlocksInChunk(const MemoryBlock &chunk, std::size_t chunk_size) {
    for (auto &stack : stacks_) {
      if (stack.empty()) {
        continue;
      }

      ::orteaf::internal::base::HeapVector<MemoryBlock> kept;
      kept.reserve(stack.size());

      while (!stack.empty()) {
        MemoryBlock top = std::move(stack.back());
        stack.resize(stack.size() - 1);
        if (!chunk.view.contains(top.view, top.view.size())) {
          kept.pushBack(std::move(top));
        }
      }

      for (std::size_t i = 0; i < kept.size(); ++i) {
        stack.pushBack(std::move(kept[i]));
      }
    }
  }

private:
  using BlockStack = ::orteaf::internal::base::HeapVector<MemoryBlock>;

  Resource *resource_{nullptr};
  ::orteaf::internal::base::HeapVector<BlockStack> stacks_{};
  std::size_t min_block_size_{64};
  std::size_t size_class_count_{0};
};

} // namespace orteaf::internal::runtime::allocator::policies

#pragma once

#include <cstddef>
#include <utility>

#include <orteaf/internal/backend/backend.h>
#include <orteaf/internal/base/heap_vector.h>
#include <orteaf/internal/diagnostics/error/error_macros.h>
#include <orteaf/internal/execution/allocator/buffer_resource.h>
#include <orteaf/internal/execution/allocator/policies/policy_config.h>
#include <orteaf/internal/execution/base/backend_traits.h>

namespace orteaf::internal::execution::allocator::policies {

/**
 * @brief Host 管理のスタック型フリーリストポリシー。
 *
 * サイズクラス別のスタックを持ち、Host 側で割り当て・解放を管理する。
 * サイズクラスの計算（min/max_block_size）は SegregatePool が担当し、
 * 本ポリシーは list_index のみを扱う。
 */
template <typename Resource>
class HostStackFreelistPolicy {
public:
  using BufferBlock = Resource::BufferBlock;
  using BufferView = BufferBlock::BufferView;
  using BufferViewHandle = BufferBlock::BufferViewHandle;
  using LaunchParams = Resource::LaunchParams;

  HostStackFreelistPolicy() = default;
  HostStackFreelistPolicy(const HostStackFreelistPolicy &) = delete;
  HostStackFreelistPolicy &operator=(const HostStackFreelistPolicy &) = delete;
  HostStackFreelistPolicy(HostStackFreelistPolicy &&) = default;
  HostStackFreelistPolicy &operator=(HostStackFreelistPolicy &&) = default;
  ~HostStackFreelistPolicy() = default;

  struct Config : PolicyConfig<Resource> {
    // サイズクラスの情報は SegregatePool が管理するため、
    // ここには含めない
  };

  /**
   * @brief ポリシーを初期化
   * @param config リソースへのポインタを含む設定
   * @param size_class_count サイズクラスの数（SegregatePool から渡される）
   */
  void initialize(const Config &config, std::size_t size_class_count = 0) {
    ORTEAF_THROW_IF_NULL(config.resource,
                         "HostStackFreelistPolicy requires non-null Resource*");
    resource_ = config.resource;
    if (size_class_count > 0) {
      stacks_.resize(size_class_count);
    }
  }

  void push(std::size_t list_index, const BufferBlock &block,
            const LaunchParams & /*launch_params*/ = {}) {
    ORTEAF_THROW_IF(resource_ == nullptr, InvalidState,
                    "HostStackFreelistPolicy is not initialized");
    ensureCapacity(list_index);
    stacks_[list_index].pushBack(block);
  }

  BufferBlock pop(std::size_t list_index,
                  const LaunchParams & /*launch_params*/ = {}) {
    ORTEAF_THROW_IF(resource_ == nullptr, InvalidState,
                    "HostStackFreelistPolicy is not initialized");
    if (list_index >= stacks_.size() || stacks_[list_index].empty()) {
      return {};
    }
    BufferBlock block = std::move(stacks_[list_index].back());
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

  void expand(std::size_t list_index, const BufferBlock &chunk,
              std::size_t chunk_size, std::size_t block_size,
              const LaunchParams & /*launch_params*/ = {}) {
    ORTEAF_THROW_IF(resource_ == nullptr, InvalidState,
                    "HostStackFreelistPolicy is not initialized");
    if (!chunk.valid() || block_size == 0) {
      return;
    }

    ensureCapacity(list_index);

    const std::size_t num_blocks = chunk_size / block_size;
    const std::size_t base_offset = chunk.view.offset();
    for (std::size_t i = 0; i < num_blocks; ++i) {
      const std::size_t offset = base_offset + i * block_size;
      BufferBlock block{chunk.handle,
                        Resource::makeView(chunk.view, offset, block_size)};
      stacks_[list_index].pushBack(std::move(block));
    }
  }

  void removeBlocksInChunk(::orteaf::internal::base::BufferViewHandle handle) {
    for (auto &stack : stacks_) {
      if (stack.empty()) {
        continue;
      }

      ::orteaf::internal::base::HeapVector<BufferBlock> kept;
      kept.reserve(stack.size());

      while (!stack.empty()) {
        BufferBlock top = std::move(stack.back());
        stack.resize(stack.size() - 1);
        if (top.handle != handle) {
          kept.pushBack(std::move(top));
        }
      }

      for (std::size_t i = 0; i < kept.size(); ++i) {
        stack.pushBack(std::move(kept[i]));
      }
    }
  }

private:
  void ensureCapacity(std::size_t list_index) {
    if (list_index >= stacks_.size()) {
      stacks_.resize(list_index + 1);
    }
  }

  using BlockStack = ::orteaf::internal::base::HeapVector<BufferBlock>;

  Resource *resource_{nullptr};
  ::orteaf::internal::base::HeapVector<BlockStack> stacks_{};
};

} // namespace orteaf::internal::execution::allocator::policies

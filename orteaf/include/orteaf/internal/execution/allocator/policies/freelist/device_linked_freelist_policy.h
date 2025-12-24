#pragma once

#include <cstddef>
#include <unordered_map>

#include <orteaf/internal/backend/backend.h>
#include <orteaf/internal/base/heap_vector.h>
#include <orteaf/internal/diagnostics/error/error_macros.h>
#include <orteaf/internal/execution/allocator/buffer_resource.h>
#include <orteaf/internal/execution/allocator/policies/policy_config.h>
#include <orteaf/internal/execution/base/backend_traits.h>

namespace orteaf::internal::execution::allocator::policies {

/**
 * @brief Device-side linked-list freelist.
 *
 * Resource がデバイス内に next 埋め込みの freelist を持ち、push/pop/expand を
 * カーネル経由で実行する前提。チャンク追加時に buffer と id の対応を保持し、
 * pop したブロックに元の BufferViewHandle を復元する。
 *
 * サイズクラスの計算（min/max_block_size）は SegregatePool が担当し、
 * 本ポリシーは list_index のみを扱う。
 */
template <typename Resource, ::orteaf::internal::backend::Backend B>
class DeviceLinkedFreelistPolicy {
public:
  using BufferResource =
      ::orteaf::internal::execution::allocator::BufferResource<B>;
  using LaunchParams =
      typename ::orteaf::internal::execution::base::BackendTraits<
          B>::KernelLaunchParams;

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
    ORTEAF_THROW_IF_NULL(
        config.resource,
        "DeviceLinkedFreelistPolicy requires non-null Resource*");
    resource_ = config.resource;
    if (size_class_count > 0) {
      heads_.resize(size_class_count);
    }
  }

  void push(std::size_t list_index, const BufferResource &block,
            const LaunchParams &launch_params = {}) {
    ORTEAF_THROW_IF(resource_ == nullptr, InvalidState,
                    "DeviceLinkedFreelistPolicy is not initialized");
    if (!block.valid())
      return;
    ensureCapacity(list_index);
    buffer_lookup_[block.view.raw()] = block.handle;
    resource_->pushFreelistNode(list_index, block.view, launch_params);
  }

  BufferResource pop(std::size_t list_index,
                     const LaunchParams &launch_params = {}) {
    ORTEAF_THROW_IF(resource_ == nullptr, InvalidState,
                    "DeviceLinkedFreelistPolicy is not initialized");
    ensureCapacity(list_index);
    auto view = resource_->popFreelistNode(list_index, launch_params);
    if (!view)
      return {};
    auto it = buffer_lookup_.find(view.raw());
    const ::orteaf::internal::base::BufferViewHandle handle =
        (it != buffer_lookup_.end())
            ? it->second
            : ::orteaf::internal::base::BufferViewHandle{};
    return BufferResource{handle, view};
  }

  bool empty(std::size_t /*list_index*/) const {
    return false;
  } // デバイス側のみで管理されるため不明

  std::size_t get_active_freelist_count() const { return heads_.size(); }

  std::size_t get_total_free_blocks() const { return 0; } // 集計不可

  void expand(std::size_t list_index, const BufferResource &chunk,
              std::size_t chunk_size, std::size_t block_size,
              const LaunchParams &launch_params = {}) {
    ORTEAF_THROW_IF(resource_ == nullptr, InvalidState,
                    "DeviceLinkedFreelistPolicy is not initialized");
    if (!chunk.valid() || block_size == 0) {
      return;
    }
    ensureCapacity(list_index);
    buffer_lookup_[chunk.view.raw()] = chunk.handle;
    resource_->initializeChunkAsFreelist(list_index, chunk.view, chunk_size,
                                         block_size, launch_params);
  }

  void
  removeBlocksInChunk(::orteaf::internal::base::BufferViewHandle /*handle*/) {
    // デバイス側のみで管理するため未対応。
  }

private:
  void ensureCapacity(std::size_t idx) {
    if (idx >= heads_.size()) {
      heads_.resize(idx + 1);
    }
  }

  Resource *resource_{nullptr};
  ::orteaf::internal::base::HeapVector<BufferResource>
      heads_{}; // unused placeholder per size class
  std::unordered_map<void *, ::orteaf::internal::base::BufferViewHandle>
      buffer_lookup_{};
};

} // namespace orteaf::internal::execution::allocator::policies

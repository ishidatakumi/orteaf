#pragma once

#include <cstddef>
#include <bit>
#include <unordered_map>

#include <orteaf/internal/backend/backend.h>
#include <orteaf/internal/runtime/base/backend_traits.h>
#include <orteaf/internal/runtime/allocator/memory_block.h>
#include <orteaf/internal/runtime/allocator/policies/policy_config.h>
#include <orteaf/internal/diagnostics/error/error_macros.h>

namespace orteaf::internal::runtime::allocator::policies {

/**
 * @brief Device-side linked-list freelist.
 *
 * Resource がデバイス内に next 埋め込みの freelist を持ち、push/pop/expand を
 * カーネル経由で実行する前提。チャンク追加時に buffer と id の対応を保持し、
 * pop したブロックに元の BufferHandle を復元する。
 *
 * 制約: Resource はスレッド安全であること（内部で head を管理）。empty/総数は
 * Resource から取得できないため概算のみ。
 */
template <typename Resource, ::orteaf::internal::backend::Backend B>
class DeviceLinkedFreelistPolicy {
public:
    using MemoryBlock = ::orteaf::internal::runtime::allocator::MemoryBlock<B>;
    using LaunchParams =
        typename ::orteaf::internal::runtime::base::BackendTraits<B>::KernelLaunchParams;

    struct Config : PolicyConfig<Resource> {
        std::size_t min_block_size{64};
        std::size_t max_block_size{0};
    };

    void initialize(const Config& config) {
        ORTEAF_THROW_IF_NULL(config.resource, "DeviceLinkedFreelistPolicy requires non-null Resource*");
        ORTEAF_THROW_IF(config.max_block_size == 0, InvalidParameter, "max_block_size must be non-zero");
        resource_ = config.resource;
        configureBounds(config.min_block_size, config.max_block_size);
    }

    void configureBounds(std::size_t min_block_size, std::size_t max_block_size) {
        ORTEAF_THROW_IF(resource_ == nullptr, InvalidState, "DeviceLinkedFreelistPolicy is not initialized");
        min_block_size_ = min_block_size;
        size_class_count_ =
            std::countr_zero(std::bit_ceil(max_block_size)) - std::countr_zero(std::bit_ceil(min_block_size)) + 1;
        heads_.resize(size_class_count_);
    }

    void push(std::size_t list_index, const MemoryBlock& block,
              const LaunchParams& launch_params = {}) {
        ORTEAF_THROW_IF(resource_ == nullptr, InvalidState, "DeviceLinkedFreelistPolicy is not initialized");
        if (!block.valid()) return;
        ensureList(list_index);
        buffer_lookup_[block.view.raw()] = block.id;
        resource_->pushFreelistNode(list_index, block.view, launch_params);
    }

    MemoryBlock pop(std::size_t list_index, const LaunchParams& launch_params = {}) {
        ORTEAF_THROW_IF(resource_ == nullptr, InvalidState, "DeviceLinkedFreelistPolicy is not initialized");
        ensureList(list_index);
        auto view = resource_->popFreelistNode(list_index, launch_params);
        if (!view) return {};
        auto it = buffer_lookup_.find(view.raw());
        const ::orteaf::internal::base::BufferHandle id = (it != buffer_lookup_.end()) ? it->second
                                                                                       : ::orteaf::internal::base::BufferHandle{};
        return MemoryBlock{id, view};
    }

    bool empty(std::size_t /*list_index*/) const { return false; }  // デバイス側のみで管理されるため不明

    std::size_t get_active_freelist_count() const { return heads_.size(); }

    std::size_t get_total_free_blocks() const { return 0; }  // 集計不可

    void expand(std::size_t list_index, const MemoryBlock& chunk, std::size_t chunk_size, std::size_t block_size,
                const LaunchParams& launch_params = {}) {
        ORTEAF_THROW_IF(resource_ == nullptr, InvalidState, "DeviceLinkedFreelistPolicy is not initialized");
        if (!chunk.valid() || block_size == 0) {
            return;
        }
        ensureList(list_index);
        buffer_lookup_[chunk.view.raw()] = chunk.id;
        resource_->initializeChunkAsFreelist(list_index, chunk.view, chunk_size, block_size, launch_params);
    }

    void removeBlocksInChunk(::orteaf::internal::base::BufferHandle /*handle*/) {
        // デバイス側のみで管理するため未対応。
    }

private:
    void ensureList(std::size_t idx) {
        if (idx >= heads_.size()) {
            heads_.resize(idx + 1);
        }
    }

    Resource* resource_{nullptr};
    std::size_t min_block_size_{64};
    std::size_t size_class_count_{0};
    ::orteaf::internal::base::HeapVector<MemoryBlock> heads_{};  // unused placeholder per size class
    std::unordered_map<void*, ::orteaf::internal::base::BufferHandle> buffer_lookup_{};
};

}  // namespace orteaf::internal::runtime::allocator::policies

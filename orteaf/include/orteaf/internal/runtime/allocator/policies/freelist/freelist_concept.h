#pragma once

#include <concepts>
#include <cstddef>

#include <orteaf/internal/backend/backend.h>
#include <orteaf/internal/runtime/base/backend_traits.h>

namespace orteaf::internal::runtime::allocator::policies {

template <typename Policy, typename Resource, ::orteaf::internal::backend::Backend B>
concept FreelistPolicy = requires(
    Policy policy, const typename Policy::Config& cfg, std::size_t list_index,
    typename Policy::MemoryBlock block, std::size_t chunk_size, std::size_t block_size,
    const typename ::orteaf::internal::runtime::base::BackendTraits<B>::KernelLaunchParams& launch_params) {
    policy.initialize(cfg);
    policy.configureBounds(chunk_size, block_size);
    policy.push(list_index, block, launch_params);
    { policy.pop(list_index, launch_params) } -> std::same_as<typename Policy::MemoryBlock>;
    policy.expand(list_index, block, chunk_size, block_size, launch_params);
    policy.removeBlocksInChunk(block.handle);
    { policy.empty(list_index) } -> std::convertible_to<bool>;
    { policy.get_active_freelist_count() } -> std::convertible_to<std::size_t>;
    { policy.get_total_free_blocks() } -> std::convertible_to<std::size_t>;
};

} // namespace orteaf::internal::runtime::allocator::policies

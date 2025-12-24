#pragma once

#include <concepts>
#include <cstddef>

namespace orteaf::internal::execution::allocator::policies {

template <typename Policy, typename Resource>
concept FreelistPolicy = requires(
    Policy policy, const typename Policy::Config& cfg, std::size_t list_index,
    typename Policy::BufferResource block, std::size_t chunk_size, std::size_t block_size,
    const typename Resource::LaunchParams& launch_params) {
    policy.initialize(cfg);
    policy.configureBounds(chunk_size, block_size);
    policy.push(list_index, block, launch_params);
    { policy.pop(list_index, launch_params) } -> std::same_as<typename Policy::BufferResource>;
    policy.expand(list_index, block, chunk_size, block_size, launch_params);
    policy.removeBlocksInChunk(block.handle);
    { policy.empty(list_index) } -> std::convertible_to<bool>;
    { policy.get_active_freelist_count() } -> std::convertible_to<std::size_t>;
    { policy.get_total_free_blocks() } -> std::convertible_to<std::size_t>;
};

} // namespace orteaf::internal::execution::allocator::policies

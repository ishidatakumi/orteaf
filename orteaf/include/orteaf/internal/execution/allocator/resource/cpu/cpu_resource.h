#pragma once

#include <cstddef>

#include "orteaf/internal/execution/cpu/resource/cpu_buffer_view.h"
#include "orteaf/internal/execution/cpu/resource/cpu_tokens.h"

namespace orteaf::internal::backend::cpu {

// CPU backend resource for direct allocation.
// For low-level heap operations (reserve/map/unmap), use CpuHeapOps.
class CpuResource {
public:
    using BufferView = ::orteaf::internal::execution::cpu::resource::CpuBufferView;
    using FenceToken = ::orteaf::internal::execution::cpu::resource::FenceToken;
    using ReuseToken = ::orteaf::internal::execution::cpu::resource::ReuseToken;

    struct Config {};

    static void initialize(const Config& config = {}) noexcept;

    static BufferView allocate(std::size_t size, std::size_t alignment);

    static void deallocate(BufferView view, std::size_t size, std::size_t alignment);

    static bool isCompleted(const FenceToken& token);
    static bool isCompleted(const ReuseToken& token);

    static BufferView makeView(BufferView base, std::size_t offset, std::size_t size);
};

}  // namespace orteaf::internal::backend::cpu

#pragma once

#include <cstddef>

#include "orteaf/internal/backend/cpu/cpu_buffer_view.h"
#include "orteaf/internal/backend/cpu/cpu_tokens.h"

namespace orteaf::internal::backend::cpu {

// CPU backend resource for direct allocation.
// For low-level heap operations (reserve/map/unmap), use CpuHeapOps.
class CpuResource {
public:
    using BufferView = ::orteaf::internal::backend::cpu::CpuBufferView;
    using FenceToken = ::orteaf::internal::backend::cpu::FenceToken;
    using ReuseToken = ::orteaf::internal::backend::cpu::ReuseToken;

    struct Config {};

    static void initialize(const Config& config = {}) noexcept;

    static BufferView allocate(std::size_t size, std::size_t alignment);

    static void deallocate(BufferView view, std::size_t size, std::size_t alignment);

    static bool isCompleted(const FenceToken& token);
    static bool isCompleted(const ReuseToken& token);
};

}  // namespace orteaf::internal::backend::cpu

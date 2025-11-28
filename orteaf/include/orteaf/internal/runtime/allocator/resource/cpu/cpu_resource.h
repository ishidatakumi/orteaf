#pragma once

#include <cstddef>

#include "orteaf/internal/backend/cpu/cpu_buffer_view.h"

namespace orteaf::internal::backend::cpu {

// CPU backend resource for direct allocation.
// For low-level heap operations (reserve/map/unmap), use CpuHeapOps.
class CpuResource {
public:
    using BufferView = ::orteaf::internal::backend::cpu::CpuBufferView;

    struct Config {};

    static void initialize(const Config& config = {}) noexcept;

    static BufferView allocate(std::size_t size, std::size_t alignment);

    static void deallocate(BufferView view, std::size_t size, std::size_t alignment);

private:
    static inline Config config_{};
    static inline bool initialized_{false};
};

}  // namespace orteaf::internal::backend::cpu

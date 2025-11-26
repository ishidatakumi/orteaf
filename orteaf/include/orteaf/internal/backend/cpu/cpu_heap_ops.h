#pragma once

#include <cstddef>

#include "orteaf/internal/backend/cpu/cpu_buffer_view.h"
#include "orteaf/internal/backend/cpu/cpu_heap_region.h"

namespace orteaf::internal::backend::cpu {

// Low-level heap operations for CPU backend.
// Used by HierarchicalChunkLocator for VA reservation and mapping.
struct CpuHeapOps {
    using BufferView = ::orteaf::internal::backend::cpu::CpuBufferView;
    using HeapRegion = ::orteaf::internal::backend::cpu::CpuHeapRegion;

    // VA reservation. Allocates PROT_NONE region via mmap.
    static HeapRegion reserve(std::size_t size);

    // Map reserved region to RW.
    static BufferView map(HeapRegion region);

    // Unmap and release the region.
    static void unmap(BufferView view, std::size_t size);
};

}  // namespace orteaf::internal::backend::cpu

#include "orteaf/internal/runtime/cpu/resource/cpu_heap_ops.h"

#include <sys/mman.h>

#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::runtime::cpu::resource {

CpuHeapOps::HeapRegion CpuHeapOps::reserve(std::size_t size) {
    if (size == 0) {
        return {};
    }
    void* base = mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (base == MAP_FAILED) {
        diagnostics::error::throwError(diagnostics::error::OrteafErrc::OutOfMemory, "cpu reserve mmap failed");
    }
    return HeapRegion{base, size};
}

CpuHeapOps::BufferView CpuHeapOps::map(HeapRegion region) {
    if (!region) return {};
    void* base = region.data();
    if (mprotect(base, region.size(), PROT_READ | PROT_WRITE) != 0) {
        diagnostics::error::throwError(diagnostics::error::OrteafErrc::OperationFailed, "cpu map mprotect failed");
    }
    return BufferView{base, 0, region.size()};
}

void CpuHeapOps::unmap(HeapRegion region, std::size_t size) {
    if (!region) return;
    void* base = region.data();
    if (munmap(base, size) != 0) {
        diagnostics::error::throwError(diagnostics::error::OrteafErrc::OperationFailed, "cpu unmap munmap failed");
    }
}

}  // namespace orteaf::internal::runtime::cpu::resource

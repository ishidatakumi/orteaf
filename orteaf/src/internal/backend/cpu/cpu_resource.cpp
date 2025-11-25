#include "orteaf/internal/backend/cpu/cpu_resource.h"

#include <sys/mman.h>
#include <unistd.h>

#include "orteaf/internal/backend/cpu/wrapper/cpu_alloc.h"
#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::backend::cpu {

void CpuResource::initialize(const Config& /*config*/) noexcept {
    // CPU backend is stateless; nothing to do.
}

CpuResource::BufferView CpuResource::reserve(std::size_t size, Device /*device*/, Stream /*stream*/) {
    if (size == 0) {
        return {};
    }
    void* base = mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (base == MAP_FAILED) {
        diagnostics::error::throwError(diagnostics::error::OrteafErrc::OutOfMemory, "cpu reserve mmap failed");
    }
    return BufferView{base, 0, size};
}

CpuResource::BufferView CpuResource::allocate(std::size_t size, std::size_t alignment,
                                              Device /*device*/, Stream /*stream*/) {
    if (size == 0) {
        return {};
    }
    void* base = cpu::allocAligned(size, alignment);
    return BufferView{base, 0, size};
}

void CpuResource::deallocate(BufferView view, std::size_t size, std::size_t /*alignment*/,
                             Device /*device*/, Stream /*stream*/) {
    if (!view) {
        return;
    }
    void* base = static_cast<void*>(static_cast<char*>(view.data()) - view.offset());
    cpu::dealloc(base, size);
}

CpuResource::BufferView CpuResource::map(BufferView view, Device /*device*/,
                                         Context /*context*/, Stream /*stream*/) {
    if (!view) return view;
    void* base = reinterpret_cast<char*>(view.data()) - view.offset();
    if (mprotect(base, view.size(), PROT_READ | PROT_WRITE) != 0) {
        diagnostics::error::throwError(diagnostics::error::OrteafErrc::OperationFailed, "cpu map mprotect failed");
    }
    return view;
}

void CpuResource::unmap(BufferView view, std::size_t size,
                        Device /*device*/, Context /*context*/, Stream /*stream*/) {
    if (!view) return;
    void* base = reinterpret_cast<char*>(view.data()) - view.offset();
    if (munmap(base, size) != 0) {
        diagnostics::error::throwError(diagnostics::error::OrteafErrc::OperationFailed, "cpu unmap munmap failed");
    }
}

}  // namespace orteaf::internal::backend::cpu

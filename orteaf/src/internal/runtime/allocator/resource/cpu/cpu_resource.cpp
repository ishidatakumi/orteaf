#include "orteaf/internal/runtime/allocator/resource/cpu/cpu_resource.h"

#include "orteaf/internal/backend/cpu/wrapper/cpu_alloc.h"

namespace orteaf::internal::backend::cpu {

void CpuResource::initialize(const Config& /*config*/) noexcept {
    // CPU backend is stateless; nothing to do.
}

CpuResource::BufferView CpuResource::allocate(std::size_t size, std::size_t alignment) {
    if (size == 0) {
        return {};
    }
    void* base = cpu::allocAligned(size, alignment);
    return BufferView{base, 0, size};
}

void CpuResource::deallocate(BufferView view, std::size_t size, std::size_t /*alignment*/) {
    if (!view) {
        return;
    }
    void* base = static_cast<void*>(static_cast<char*>(view.data()) - view.offset());
    cpu::dealloc(base, size);
}

}  // namespace orteaf::internal::backend::cpu

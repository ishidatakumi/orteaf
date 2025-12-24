#include "orteaf/internal/execution/allocator/resource/cpu/cpu_resource.h"

#include "orteaf/internal/execution/cpu/platform/wrapper/cpu_alloc.h"
#include "orteaf/internal/diagnostics/error/error_macros.h"

namespace orteaf::internal::backend::cpu {
namespace cpu = ::orteaf::internal::execution::cpu::platform::wrapper;

void CpuResource::initialize(const Config& /*config*/) noexcept {
    // Stateless; nothing to do.
}

CpuResource::BufferView CpuResource::allocate(std::size_t size, std::size_t alignment) {
    ORTEAF_THROW_IF(size == 0, InvalidParameter, "CpuResource::allocate requires size > 0");
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

bool CpuResource::isCompleted(const FenceToken& token) {
    (void)token;
    return true;
}

bool CpuResource::isCompleted(const ReuseToken& token) {
    (void)token;
    return true;
}

CpuResource::BufferView CpuResource::makeView(BufferView base, std::size_t offset, std::size_t size) {
    return BufferView{base.raw(), offset, size};
}

}  // namespace orteaf::internal::backend::cpu

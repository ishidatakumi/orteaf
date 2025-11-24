#pragma once

#include <cstddef>

#include "orteaf/internal/backend/backend_traits.h"
#include "orteaf/internal/backend/cpu/cpu_buffer_view.h"
#include "orteaf/internal/backend/cpu/wrapper/cpu_alloc.h"

namespace orteaf::internal::backend::cpu {

// CPU backend resource used by allocator policies; non-owning BufferView wrapper.
struct CpuResource {
    using BufferView = ::orteaf::internal::backend::cpu::CpuBufferView;
    using Device = ::orteaf::internal::backend::BackendTraits<::orteaf::internal::backend::Backend::Cpu>::Device;
    using Context = ::orteaf::internal::backend::BackendTraits<::orteaf::internal::backend::Backend::Cpu>::Context;
    using Stream = ::orteaf::internal::backend::BackendTraits<::orteaf::internal::backend::Backend::Cpu>::Stream;

    static BufferView allocate(std::size_t size, std::size_t alignment, Device /*device*/, Stream /*stream*/) {
        if (size == 0) {
            return {};
        }
        void* base = cpu::allocAligned(size, alignment);
        return BufferView{base, 0, size};
    }

    static void deallocate(BufferView view, std::size_t size, std::size_t /*alignment*/, Device /*device*/, Stream /*stream*/) {
        if (!view) {
            return;
        }
        // Reconstruct the original base pointer from view data/offset.
        void* base = static_cast<void*>(static_cast<char*>(view.data()) - view.offset());
        cpu::dealloc(base, size);
    }
};

}  // namespace orteaf::internal::backend::cpu

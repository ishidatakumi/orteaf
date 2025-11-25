#pragma once

#include <cstddef>

#include "orteaf/internal/backend/backend_traits.h"
#include "orteaf/internal/backend/cpu/cpu_buffer_view.h"

namespace orteaf::internal::backend::cpu {

// CPU backend resource used by allocator policies; non-owning BufferView wrapper.
struct CpuResource {
    using BufferView = ::orteaf::internal::backend::cpu::CpuBufferView;
    using Device = ::orteaf::internal::backend::BackendTraits<::orteaf::internal::backend::Backend::Cpu>::Device;
    using Context = ::orteaf::internal::backend::BackendTraits<::orteaf::internal::backend::Backend::Cpu>::Context;
    using Stream = ::orteaf::internal::backend::BackendTraits<::orteaf::internal::backend::Backend::Cpu>::Stream;

    struct Config {};

    static void initialize(const Config& config = {}) noexcept;

    // VA 予約。mmap で PROT_NONE の領域を確保し、PA は map で張る。
    static BufferView reserve(std::size_t size, Device device, Stream stream);

    static BufferView allocate(std::size_t size, std::size_t alignment, Device device, Stream stream);

    static void deallocate(BufferView view, std::size_t size, std::size_t alignment, Device device, Stream stream);

    // map/unmap は VA を RW に切り替え、unmap で解放する。
    static BufferView map(BufferView view, Device device, Context context, Stream stream);

    static void unmap(BufferView view, std::size_t size, Device device, Context context, Stream stream);
};

}  // namespace orteaf::internal::backend::cpu

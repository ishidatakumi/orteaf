#include "orteaf/internal/runtime/allocator/resource/mps/mps_resource.h"

#if ORTEAF_ENABLE_MPS

namespace orteaf::internal::backend::mps {

void MpsResource::initialize(const Config& config) noexcept {
    state_.device = config.device;
    state_.heap = config.heap;
    state_.usage = config.usage;
    state_.initialized = (state_.device != nullptr && state_.heap != nullptr);
}

MpsResource::BufferView MpsResource::allocate(std::size_t size, std::size_t /*alignment*/) {
    if (!state_.initialized || size == 0) {
        return {};
    }

    MPSBuffer_t buffer = createBuffer(state_.heap, size, state_.usage);
    if (!buffer) {
        return {};
    }

    return BufferView{buffer, 0, size};
}

void MpsResource::deallocate(BufferView view, std::size_t /*size*/, std::size_t /*alignment*/) noexcept {
    if (!view) {
        return;
    }
    destroyBuffer(view.raw());
}

}  // namespace orteaf::internal::backend::mps

#endif  // ORTEAF_ENABLE_MPS

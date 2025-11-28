#include "orteaf/internal/runtime/allocator/resource/mps/mps_resource.h"

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/diagnostics/error/error_macros.h"

namespace orteaf::internal::backend::mps {

void MpsResource::initialize(const Config& config) {
    ORTEAF_THROW_IF_NULL(config.device, "MpsResource requires non-null device");
    ORTEAF_THROW_IF_NULL(config.heap, "MpsResource requires non-null heap");
    device_ = config.device;
    heap_ = config.heap;
    usage_ = config.usage;
    initialized_ = (device_ != nullptr && heap_ != nullptr);
}

MpsResource::BufferView MpsResource::allocate(std::size_t size, std::size_t /*alignment*/) {
    ORTEAF_THROW_IF(!initialized_, InvalidState, "MpsResource::allocate called before initialize");
    ORTEAF_THROW_IF(size == 0, InvalidParameter, "MpsResource::allocate requires size > 0");

    MPSBuffer_t buffer = createBuffer(heap_, size, usage_);
    if (!buffer) {
        return {};
    }

    return BufferView{buffer, 0, size};
}

void MpsResource::deallocate(BufferView view, std::size_t /*size*/, std::size_t /*alignment*/) noexcept {
    if (!initialized_ || !view) {
        return;
    }
    destroyBuffer(view.raw());
}

}  // namespace orteaf::internal::backend::mps

#endif  // ORTEAF_ENABLE_MPS

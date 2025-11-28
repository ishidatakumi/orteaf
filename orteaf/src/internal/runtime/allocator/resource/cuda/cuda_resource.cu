#include "orteaf/internal/runtime/allocator/resource/cuda/cuda_resource.h"

#include "orteaf/internal/backend/cuda/wrapper/cuda_alloc.h"
#include "orteaf/internal/diagnostics/error/error_macros.h"

namespace orteaf::internal::backend::cuda {

void CudaResource::initialize(const Config& config) noexcept {
    (void)config;
}

CudaResource::BufferView CudaResource::allocate(std::size_t size, std::size_t /*alignment*/) {
    ORTEAF_THROW_IF(size == 0, InvalidParameter, "CudaResource::allocate requires size > 0");

    CUdeviceptr_t base = ::orteaf::internal::backend::cuda::alloc(size);
    if (base == 0) {
        return {};
    }
    return BufferView{base, 0, size};
}

void CudaResource::deallocate(BufferView view, std::size_t size, std::size_t /*alignment*/) {
    if (!view) {
        return;
    }
    const auto base = view.data() - static_cast<CUdeviceptr_t>(view.offset());
    ::orteaf::internal::backend::cuda::free(base, size);
}

}  // namespace orteaf::internal::backend::cuda

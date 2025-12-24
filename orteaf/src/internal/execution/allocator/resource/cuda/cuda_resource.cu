#include "orteaf/internal/execution/allocator/resource/cuda/cuda_resource.h"

#include "orteaf/internal/diagnostics/error/error_macros.h"
#include "orteaf/internal/execution/cuda/platform/wrapper/cuda_alloc.h"

namespace orteaf::internal::execution::cuda::resource {

void CudaResource::initialize(const Config &config) noexcept { (void)config; }

CudaResource::BufferView CudaResource::allocate(std::size_t size,
                                                std::size_t /*alignment*/) {
  ORTEAF_THROW_IF(size == 0, InvalidParameter,
                  "CudaResource::allocate requires size > 0");

  auto base =
      ::orteaf::internal::execution::cuda::platform::wrapper::alloc(size);
  if (base == 0) {
    return {};
  }
  return BufferView{base, 0, size};
}

void CudaResource::deallocate(BufferView view, std::size_t size,
                              std::size_t /*alignment*/) {
  if (!view) {
    return;
  }
  const auto base = view.data() - static_cast<::orteaf::internal::execution::cuda::platform::wrapper::CudaDevicePtr_t>(view.offset());
  ::orteaf::internal::execution::cuda::platform::wrapper::free(base, size);
}

bool CudaResource::isCompleted(const FenceToken &token) {
  (void)token;
  return true;
}

bool CudaResource::isCompleted(const ReuseToken &token) {
  (void)token;
  return true;
}

CudaResource::BufferView
CudaResource::makeView(BufferView base, std::size_t offset, std::size_t size) {
  return BufferView{base.raw(), offset, size};
}

} // namespace orteaf::internal::execution::cuda::resource

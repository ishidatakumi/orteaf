#pragma once

#include <variant>

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/runtime/allocator/memory_block.h"

namespace orteaf::internal::runtime::allocator {

// Type-erased wrapper around backend-specific BufferResource.
struct Buffer {
  using CpuResource = BufferResource<::orteaf::internal::backend::Backend::Cpu>;
#if ORTEAF_ENABLE_CUDA
  using CudaResource =
      BufferResource<::orteaf::internal::backend::Backend::Cuda>;
#endif
#if ORTEAF_ENABLE_MPS
  using MpsResource = BufferResource<::orteaf::internal::backend::Backend::Mps>;
#endif

  using ResourceVariant = std::variant<
      CpuResource
#if ORTEAF_ENABLE_CUDA
      , CudaResource
#endif
#if ORTEAF_ENABLE_MPS
      , MpsResource
#endif
      >;

  Buffer() = default;

  template <::orteaf::internal::backend::Backend B>
  explicit Buffer(const BufferResource<B> &res) : backend(B), resource(res) {}

  ::orteaf::internal::backend::Backend backend{
      ::orteaf::internal::backend::Backend::Cpu};
  ResourceVariant resource{};

  bool valid() const {
    return std::visit([](const auto &r) { return r.valid(); }, resource);
  }

  template <::orteaf::internal::backend::Backend B>
  BufferResource<B> asResource() const {
    const auto *r = std::get_if<BufferResource<B>>(&resource);
    return (r && backend == B) ? *r : BufferResource<B>{};
  }

  template <::orteaf::internal::backend::Backend B>
  explicit operator BufferResource<B>() const {
    return asResource<B>();
  }
};

} // namespace orteaf::internal::runtime::allocator

#pragma once

#include <cstddef>
#include <variant>

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/execution/allocator/buffer_resource.h"

namespace orteaf::internal::execution::allocator {

// Type-erased wrapper around backend-specific BufferResource.
class Buffer {
  using CpuResource = BufferResource<::orteaf::internal::backend::Backend::Cpu>;
#if ORTEAF_ENABLE_CUDA
  using CudaResource =
      BufferResource<::orteaf::internal::backend::Backend::Cuda>;
#endif
#if ORTEAF_ENABLE_MPS
  using MpsResource = BufferResource<::orteaf::internal::backend::Backend::Mps>;
#endif

  using ResourceVariant = std::variant<CpuResource
#if ORTEAF_ENABLE_CUDA
                                       ,
                                       CudaResource
#endif
#if ORTEAF_ENABLE_MPS
                                       ,
                                       MpsResource
#endif
                                       >;

public:
  Buffer() = default;

  // Move-only (BufferResource is not copyable)
  Buffer(const Buffer &) = delete;
  Buffer &operator=(const Buffer &) = delete;
  Buffer(Buffer &&) = default;
  Buffer &operator=(Buffer &&) = default;

  template <::orteaf::internal::backend::Backend B>
  explicit Buffer(BufferResource<B> res, std::size_t size_bytes = 0,
                  std::size_t alignment_bytes = 0)
      : backend_(B), resource_(std::move(res)), size_(size_bytes),
        alignment_(alignment_bytes) {}

  ::orteaf::internal::backend::Backend backend() const noexcept {
    return backend_;
  }
  std::size_t size() const noexcept { return size_; }
  std::size_t alignment() const noexcept { return alignment_; }

  bool valid() const {
    return std::visit([](const auto &r) { return r.valid(); }, resource_);
  }

  template <::orteaf::internal::backend::Backend B>
  BufferResource<B> &asResource() {
    auto *r = std::get_if<BufferResource<B>>(&resource_);
    static BufferResource<B> empty{};
    return (r && backend_ == B) ? *r : empty;
  }

  template <::orteaf::internal::backend::Backend B>
  const BufferResource<B> &asResource() const {
    const auto *r = std::get_if<BufferResource<B>>(&resource_);
    static const BufferResource<B> empty{};
    return (r && backend_ == B) ? *r : empty;
  }

private:
  ::orteaf::internal::backend::Backend backend_{
      ::orteaf::internal::backend::Backend::Cpu};
  ResourceVariant resource_{};
  std::size_t size_{0};
  std::size_t alignment_{0};
};

} // namespace orteaf::internal::execution::allocator

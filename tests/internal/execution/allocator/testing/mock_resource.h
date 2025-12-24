#pragma once

#include <cstddef>

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/execution/allocator/buffer_resource.h"
#include "orteaf/internal/execution/cpu/resource/cpu_buffer_view.h"
#include "orteaf/internal/execution/cpu/resource/cpu_heap_region.h"
#include <gmock/gmock.h>

namespace orteaf::internal::execution::allocator::testing {

// ============================================================================
// MockCpuHeapOpsImpl - gMock-able implementation for HeapOps
// ============================================================================
class MockCpuHeapOpsImpl {
public:
  using BufferView = ::orteaf::internal::execution::cpu::resource::CpuBufferView;
  using HeapRegion = ::orteaf::internal::execution::cpu::resource::CpuHeapRegion;

  MOCK_METHOD(HeapRegion, reserve, (std::size_t size));
  MOCK_METHOD(BufferView, map, (HeapRegion region));
  MOCK_METHOD(void, unmap, (HeapRegion region, std::size_t size));
};

// Static-API wrapper that forwards to a shared MockCpuHeapOpsImpl instance.
struct MockCpuHeapOps {
  using BufferView = ::orteaf::internal::execution::cpu::resource::CpuBufferView;
  using HeapRegion = ::orteaf::internal::execution::cpu::resource::CpuHeapRegion;

  static void set(MockCpuHeapOpsImpl *impl) { impl_ = impl; }
  static void reset() { impl_ = nullptr; }

  static HeapRegion reserve(std::size_t size) {
    return impl_ ? impl_->reserve(size) : HeapRegion{};
  }
  static BufferView map(HeapRegion region) {
    return impl_ ? impl_->map(region) : BufferView{};
  }
  static void unmap(HeapRegion region, std::size_t size) {
    if (impl_)
      impl_->unmap(region, size);
  }

private:
  static inline MockCpuHeapOpsImpl *impl_{nullptr};
};

// ============================================================================
// MockCpuResourceImpl - gMock-able implementation for Resource
// ============================================================================
class MockCpuResourceImpl {
public:
  using BufferView = ::orteaf::internal::execution::cpu::resource::CpuBufferView;

  MOCK_METHOD(BufferView, allocate, (std::size_t size, std::size_t alignment));
  MOCK_METHOD(void, deallocate,
              (BufferView view, std::size_t size, std::size_t alignment));
  MOCK_METHOD(BufferView, makeView,
              (BufferView base, std::size_t offset, std::size_t size));
};

// Static-API wrapper that forwards to a shared MockCpuResourceImpl instance.
struct MockCpuResource {
  using BufferView = ::orteaf::internal::execution::cpu::resource::CpuBufferView;
  using BufferBlock = ::orteaf::internal::execution::allocator::BufferBlock<
      ::orteaf::internal::backend::Backend::Cpu>;
  struct LaunchParams {};

  static void set(MockCpuResourceImpl *impl) { impl_ = impl; }
  static void reset() { impl_ = nullptr; }

  static BufferView allocate(std::size_t size, std::size_t alignment) {
    return impl_ ? impl_->allocate(size, alignment) : BufferView{};
  }
  static void deallocate(BufferView view, std::size_t size,
                         std::size_t alignment) {
    if (impl_)
      impl_->deallocate(view, size, alignment);
  }
  static BufferView makeView(BufferView base, std::size_t offset,
                             std::size_t size) {
    return impl_ ? impl_->makeView(base, offset, size)
                 : BufferView{base.raw(), offset, size};
  }

private:
  static inline MockCpuResourceImpl *impl_{nullptr};
};

} // namespace orteaf::internal::execution::allocator::testing

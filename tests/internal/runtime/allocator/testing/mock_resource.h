#pragma once

#include <cstddef>

#include <gmock/gmock.h>
#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/backend/backend_traits.h"
#include "orteaf/internal/backend/cpu/cpu_buffer_view.h"
#include "orteaf/internal/backend/cpu/cpu_heap_region.h"

namespace orteaf::internal::runtime::allocator::testing {

// gMock-able implementation
class MockCpuResourceImpl {
public:
    using BufferView = ::orteaf::internal::backend::cpu::CpuBufferView;
    using HeapRegion = ::orteaf::internal::backend::cpu::CpuHeapRegion;
    using Stream = ::orteaf::internal::backend::BackendTraits<::orteaf::internal::backend::Backend::Cpu>::Stream;
    MOCK_METHOD(HeapRegion, reserve,
                (std::size_t size, Stream stream));
    MOCK_METHOD(BufferView, allocate,
                (std::size_t size, std::size_t alignment, Stream stream));
    MOCK_METHOD(void, deallocate,
                (BufferView view, std::size_t size, std::size_t alignment, Stream stream));
    MOCK_METHOD(BufferView, map, (HeapRegion region, Stream stream));
    MOCK_METHOD(void, unmap, (BufferView view, std::size_t size, Stream stream));
};

// Static-API wrapper that forwards to a shared MockCpuResourceImpl instance.
struct MockCpuResource {
    using BufferView = ::orteaf::internal::backend::cpu::CpuBufferView;
    using HeapRegion = ::orteaf::internal::backend::cpu::CpuHeapRegion;
    using Stream = MockCpuResourceImpl::Stream;

    static void set(MockCpuResourceImpl* impl) { impl_ = impl; }
    static void reset() { impl_ = nullptr; }

    static HeapRegion reserve(std::size_t size, Stream stream) {
        return impl_ ? impl_->reserve(size, stream) : HeapRegion{};
    }
    static BufferView allocate(std::size_t size, std::size_t alignment, Stream stream) {
        return impl_ ? impl_->allocate(size, alignment, stream) : BufferView{};
    }
    static void deallocate(BufferView view, std::size_t size, std::size_t alignment, Stream stream) {
        if (impl_) impl_->deallocate(view, size, alignment, stream);
    }

    static BufferView map(HeapRegion region, Stream stream) {
        return impl_ ? impl_->map(region, stream) : BufferView{};
    }

    static void unmap(BufferView view, std::size_t size, Stream stream) {
        if (impl_) impl_->unmap(view, size, stream);
    }

private:
    static inline MockCpuResourceImpl* impl_{nullptr};
};

}  // namespace orteaf::internal::runtime::allocator::testing

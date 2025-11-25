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
    using Device = ::orteaf::internal::backend::BackendTraits<::orteaf::internal::backend::Backend::Cpu>::Device;
    using Stream = ::orteaf::internal::backend::BackendTraits<::orteaf::internal::backend::Backend::Cpu>::Stream;
    using Context = ::orteaf::internal::backend::BackendTraits<::orteaf::internal::backend::Backend::Cpu>::Context;
    MOCK_METHOD(HeapRegion, reserve,
                (std::size_t size, Device device, Stream stream));
    MOCK_METHOD(BufferView, allocate,
                (std::size_t size, std::size_t alignment, Device device, Stream stream));
    MOCK_METHOD(void, deallocate,
                (BufferView view, std::size_t size, std::size_t alignment, Device device, Stream stream));
    MOCK_METHOD(BufferView, map, (HeapRegion region, Device device, Context context, Stream stream));
    MOCK_METHOD(void, unmap, (HeapRegion region, std::size_t size, Device device, Context context, Stream stream));
};

// Static-API wrapper that forwards to a shared MockCpuResourceImpl instance.
struct MockCpuResource {
    using BufferView = ::orteaf::internal::backend::cpu::CpuBufferView;
    using HeapRegion = ::orteaf::internal::backend::cpu::CpuHeapRegion;
    using Device = MockCpuResourceImpl::Device;
    using Stream = MockCpuResourceImpl::Stream;
    using Context = MockCpuResourceImpl::Context;

    static void set(MockCpuResourceImpl* impl) { impl_ = impl; }
    static void reset() { impl_ = nullptr; }

    static HeapRegion reserve(std::size_t size, Device device, Stream stream) {
        return impl_ ? impl_->reserve(size, device, stream) : HeapRegion{};
    }
    static BufferView allocate(std::size_t size, std::size_t alignment, Device device, Stream stream) {
        return impl_ ? impl_->allocate(size, alignment, device, stream) : BufferView{};
    }
    static void deallocate(BufferView view, std::size_t size, std::size_t alignment, Device device, Stream stream) {
        if (impl_) impl_->deallocate(view, size, alignment, device, stream);
    }

    static BufferView map(HeapRegion region, Device device, Context context, Stream stream) {
        return impl_ ? impl_->map(region, device, context, stream) : BufferView{};
    }

    static void unmap(HeapRegion region, std::size_t size, Device device, Context context,
                      Stream stream) {
        if (impl_) impl_->unmap(region, size, device, context, stream);
    }

private:
    static inline MockCpuResourceImpl* impl_{nullptr};
};

}  // namespace orteaf::internal::runtime::allocator::testing

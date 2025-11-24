#pragma once

#include <cstddef>

#include <gmock/gmock.h>
#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/backend/backend_traits.h"
#include "orteaf/internal/backend/cpu/cpu_buffer_view.h"

namespace orteaf::internal::runtime::allocator::testing {

// gMock-able implementation
class MockCpuResourceImpl {
public:
    using BufferView = ::orteaf::internal::backend::cpu::CpuBufferView;
    using Device = ::orteaf::internal::backend::BackendTraits<::orteaf::internal::backend::Backend::Cpu>::Device;
    using Stream = ::orteaf::internal::backend::BackendTraits<::orteaf::internal::backend::Backend::Cpu>::Stream;
    MOCK_METHOD(BufferView, allocate,
                (std::size_t size, std::size_t alignment, Device device, Stream stream));
    MOCK_METHOD(void, deallocate,
                (BufferView view, std::size_t size, std::size_t alignment, Device device, Stream stream));
};

// Static-API wrapper that forwards to a shared MockCpuResourceImpl instance.
struct MockCpuResource {
    using BufferView = ::orteaf::internal::backend::cpu::CpuBufferView;
    using Device = MockCpuResourceImpl::Device;
    using Stream = MockCpuResourceImpl::Stream;

    static void set(MockCpuResourceImpl* impl) { impl_ = impl; }
    static void reset() { impl_ = nullptr; }

    static BufferView allocate(std::size_t size, std::size_t alignment, Device device, Stream stream) {
        return impl_ ? impl_->allocate(size, alignment, device, stream) : BufferView{};
    }
    static void deallocate(BufferView view, std::size_t size, std::size_t alignment, Device device, Stream stream) {
        if (impl_) impl_->deallocate(view, size, alignment, device, stream);
    }

private:
    static inline MockCpuResourceImpl* impl_{nullptr};
};

}  // namespace orteaf::internal::runtime::allocator::testing

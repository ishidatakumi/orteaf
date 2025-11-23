#pragma once

#include <cstddef>

#include <gmock/gmock.h>
#include "orteaf/internal/backend/cpu/cpu_buffer_view.h"

namespace orteaf::internal::runtime::allocator::testing {

// gMock-able implementation
class MockCpuResourceImpl {
public:
    using BufferView = ::orteaf::internal::backend::cpu::CpuBufferView;
    MOCK_METHOD(BufferView, allocate, (std::size_t size, std::size_t alignment));
    MOCK_METHOD(void, deallocate, (BufferView view, std::size_t size, std::size_t alignment));
};

// Static-API wrapper that forwards to a shared MockCpuResourceImpl instance.
struct MockCpuResource {
    using BufferView = ::orteaf::internal::backend::cpu::CpuBufferView;

    static void set(MockCpuResourceImpl* impl) { impl_ = impl; }
    static void reset() { impl_ = nullptr; }

    static BufferView allocate(std::size_t size, std::size_t alignment) {
        return impl_ ? impl_->allocate(size, alignment) : BufferView{};
    }
    static void deallocate(BufferView view, std::size_t size, std::size_t alignment) {
        if (impl_) impl_->deallocate(view, size, alignment);
    }

private:
    static inline MockCpuResourceImpl* impl_{nullptr};
};

}  // namespace orteaf::internal::runtime::allocator::testing

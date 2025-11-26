#pragma once

#include <cstddef>

#include <gmock/gmock.h>
#include "orteaf/internal/backend/cpu/cpu_buffer_view.h"
#include "orteaf/internal/backend/cpu/cpu_heap_region.h"

namespace orteaf::internal::runtime::allocator::testing {

// ============================================================================
// MockCpuHeapOpsImpl - gMock-able implementation for HeapOps
// ============================================================================
class MockCpuHeapOpsImpl {
public:
    using BufferView = ::orteaf::internal::backend::cpu::CpuBufferView;
    using HeapRegion = ::orteaf::internal::backend::cpu::CpuHeapRegion;

    MOCK_METHOD(HeapRegion, reserve, (std::size_t size));
    MOCK_METHOD(BufferView, map, (HeapRegion region));
    MOCK_METHOD(void, unmap, (BufferView view, std::size_t size));
};

// Static-API wrapper that forwards to a shared MockCpuHeapOpsImpl instance.
struct MockCpuHeapOps {
    using BufferView = ::orteaf::internal::backend::cpu::CpuBufferView;
    using HeapRegion = ::orteaf::internal::backend::cpu::CpuHeapRegion;

    static void set(MockCpuHeapOpsImpl* impl) { impl_ = impl; }
    static void reset() { impl_ = nullptr; }

    static HeapRegion reserve(std::size_t size) {
        return impl_ ? impl_->reserve(size) : HeapRegion{};
    }
    static BufferView map(HeapRegion region) {
        return impl_ ? impl_->map(region) : BufferView{};
    }
    static void unmap(BufferView view, std::size_t size) {
        if (impl_) impl_->unmap(view, size);
    }

private:
    static inline MockCpuHeapOpsImpl* impl_{nullptr};
};

// ============================================================================
// MockCpuResourceImpl - gMock-able implementation for Resource
// ============================================================================
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

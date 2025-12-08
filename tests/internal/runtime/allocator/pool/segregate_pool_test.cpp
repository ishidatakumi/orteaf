#include "orteaf/internal/runtime/allocator/pool/segregate_pool.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/runtime/allocator/memory_block.h"
#include "orteaf/internal/runtime/allocator/policies/chunk_locator/direct_chunk_locator.h"
#include "orteaf/internal/runtime/allocator/policies/fast_free/fast_free_policies.h"
#include "orteaf/internal/runtime/allocator/policies/freelist/host_stack_freelist_policy.h"
#include "orteaf/internal/runtime/allocator/policies/large_alloc/direct_resource_large_alloc.h"
#include "orteaf/internal/runtime/allocator/policies/policy_config.h"
#include "orteaf/internal/runtime/allocator/policies/reuse/deferred_reuse_policy.h"
#include "orteaf/internal/runtime/allocator/policies/threading/threading_policies.h"
#include "orteaf/internal/runtime/base/backend_traits.h"
#include "orteaf/internal/runtime/cpu/resource/cpu_buffer_view.h"
#include "tests/internal/runtime/allocator/testing/mock_resource.h"

namespace {

using ::orteaf::internal::backend::Backend;
using ::orteaf::internal::runtime::allocator::MemoryBlock;
using ::orteaf::internal::runtime::allocator::testing::MockCpuResourceImpl;
using ::orteaf::internal::runtime::allocator::testing::MockCpuResource;
namespace policies = ::orteaf::internal::runtime::allocator::policies;
using CpuBufferView = ::orteaf::internal::runtime::cpu::resource::CpuBufferView;
using ::testing::NiceMock;
using ::testing::Return;

// ---------------------------------------------------------------------------
// Test doubles
// ---------------------------------------------------------------------------
struct MockResource {
    using BufferView = CpuBufferView;
    using MemoryBlock = ::orteaf::internal::runtime::allocator::MemoryBlock<Backend::Cpu>;
    using ReuseToken = ::orteaf::internal::runtime::base::BackendTraits<Backend::Cpu>::ReuseToken;

    static void set(MockCpuResourceImpl* impl) { MockCpuResource::set(impl); }
    static void reset() { MockCpuResource::reset(); }

    static BufferView allocate(std::size_t size, std::size_t alignment) {
        return MockCpuResource::allocate(size, alignment);
    }

    static void deallocate(BufferView view, std::size_t size, std::size_t alignment) {
        MockCpuResource::deallocate(view, size, alignment);
    }

    static BufferView makeView(BufferView base, std::size_t offset, std::size_t size) {
        return MockCpuResource::makeView(base, offset, size);
    }

    static bool isCompleted(const ReuseToken&) { return true; }
};

struct MockResourceGuard {
    explicit MockResourceGuard(MockCpuResourceImpl* impl) { MockResource::set(impl); }
    ~MockResourceGuard() { MockResource::reset(); }
};

using Pool = ::orteaf::internal::runtime::allocator::pool::SegregatePool<
    MockResource,
    policies::FastFreePolicy,
    policies::NoLockThreadingPolicy,
    policies::DirectResourceLargeAllocPolicy<MockResource, Backend::Cpu>,
    policies::DirectChunkLocatorPolicy<MockResource, Backend::Cpu>,
    policies::DeferredReusePolicy<MockResource, Backend::Cpu>,
    policies::HostStackFreelistPolicy<MockResource, Backend::Cpu>,
    Backend::Cpu>;

TEST(SegregatePool, InitializePropagatesToAllPolicies) {
    Pool pool;
    Pool::Config cfg{};
    MockResource resource{};
    NiceMock<MockCpuResourceImpl> impl;
    MockResourceGuard guard(&impl);

    cfg.fast_free.resource = &resource;
    cfg.threading.resource = &resource;
    cfg.large_alloc.resource = &resource;
    cfg.chunk_locator.resource = &resource;
    cfg.reuse.resource = &resource;
    cfg.freelist.resource = &resource;
    cfg.min_block_size = 64;
    cfg.max_block_size = 256;
    cfg.freelist.min_block_size = cfg.min_block_size;
    cfg.freelist.max_block_size = cfg.max_block_size;

    EXPECT_CALL(impl, allocate).Times(0);

    pool.initialize(cfg);

    EXPECT_EQ(pool.free_list_policy().get_active_freelist_count(), 1u);
    EXPECT_TRUE(pool.free_list_policy().empty(0));
}

TEST(SegregatePool, AllocatesFromChunkWhenBelowMaxSize) {
    Pool pool;
    Pool::Config cfg{};
    MockResource resource{};
    NiceMock<MockCpuResourceImpl> impl;
    MockResourceGuard guard(&impl);
    ON_CALL(impl, makeView).WillByDefault([](CpuBufferView base, std::size_t offset, std::size_t size) {
        return CpuBufferView{base.raw(), offset, size};
    });

    cfg.fast_free.resource = &resource;
    cfg.threading.resource = &resource;
    cfg.large_alloc.resource = &resource;
    cfg.chunk_locator.resource = &resource;
    cfg.reuse.resource = &resource;
    cfg.freelist.resource = &resource;
    cfg.chunk_size = 256;
    cfg.min_block_size = 64;
    cfg.max_block_size = 256;
    cfg.freelist.min_block_size = cfg.min_block_size;
    cfg.freelist.max_block_size = cfg.max_block_size;
    pool.initialize(cfg);

    // One chunk allocation; freelist should hand back a block view aligned to block_size.
    void* base = reinterpret_cast<void*>(0x1000);
    const std::size_t block_size = 128; // ceil(max(64, 80))
    EXPECT_CALL(impl, allocate(256, 0)).WillOnce(Return(CpuBufferView{base, 0, 256}));

    Pool::LaunchParams params{};
    MemoryBlock block = pool.allocate(80, 64, params);

    EXPECT_TRUE(block.valid());
    EXPECT_EQ(block.view.size(), block_size);
    EXPECT_LT(block.view.offset(), cfg.chunk_size);
}

TEST(SegregatePool, UsesLargeAllocWhenOverMaxSize) {
    Pool pool;
    Pool::Config cfg{};
    MockResource resource{};
    NiceMock<MockCpuResourceImpl> impl;
    MockResourceGuard guard(&impl);
    ON_CALL(impl, makeView).WillByDefault([](CpuBufferView base, std::size_t offset, std::size_t size) {
        return CpuBufferView{base.raw(), offset, size};
    });

    cfg.fast_free.resource = &resource;
    cfg.threading.resource = &resource;
    cfg.large_alloc.resource = &resource;
    cfg.chunk_locator.resource = &resource;
    cfg.reuse.resource = &resource;
    cfg.freelist.resource = &resource;
    cfg.chunk_size = 256;
    cfg.min_block_size = 64;
    cfg.max_block_size = 128;
    cfg.freelist.min_block_size = cfg.min_block_size;
    cfg.freelist.max_block_size = cfg.max_block_size;
    pool.initialize(cfg);

    void* base = reinterpret_cast<void*>(0x2000);
    EXPECT_CALL(impl, allocate(300, 16)).WillOnce(Return(CpuBufferView{base, 0, 300}));

    Pool::LaunchParams params{};
    MemoryBlock block = pool.allocate(300, 16, params);

    EXPECT_TRUE(block.valid());
    EXPECT_EQ(block.view.size(), 300u);
    EXPECT_EQ(block.view.offset(), 0u);
}

TEST(SegregatePool, DeallocateReturnsBlockToFreelist) {
    Pool pool;
    Pool::Config cfg{};
    MockResource resource{};
    NiceMock<MockCpuResourceImpl> impl;
    MockResourceGuard guard(&impl);
    ON_CALL(impl, makeView).WillByDefault([](CpuBufferView base, std::size_t offset, std::size_t size) {
        return CpuBufferView{base.raw(), offset, size};
    });

    cfg.fast_free.resource = &resource;
    cfg.threading.resource = &resource;
    cfg.large_alloc.resource = &resource;
    cfg.chunk_locator.resource = &resource;
    cfg.reuse.resource = &resource;
    cfg.freelist.resource = &resource;
    cfg.chunk_size = 256;
    cfg.min_block_size = 64;
    cfg.max_block_size = 256;
    cfg.freelist.min_block_size = cfg.min_block_size;
    cfg.freelist.max_block_size = cfg.max_block_size;
    pool.initialize(cfg);

    void* base = reinterpret_cast<void*>(0x3000);
    EXPECT_CALL(impl, allocate(256, 0)).WillOnce(Return(CpuBufferView{base, 0, 256}));

    Pool::LaunchParams params{};
    MemoryBlock block = pool.allocate(80, 64, params);
    ASSERT_TRUE(block.valid());
    testing::Mock::VerifyAndClearExpectations(&impl);

    pool.deallocate(block, 80, 64, params);

    EXPECT_CALL(impl, allocate).Times(0);
    MemoryBlock reused = pool.allocate(80, 64, params);
    EXPECT_TRUE(reused.valid());
    EXPECT_EQ(reused.view.size(), block.view.size());
}

TEST(SegregatePool, DeallocateLargeAllocUsesLargePolicy) {
    Pool pool;
    Pool::Config cfg{};
    MockResource resource{};
    NiceMock<MockCpuResourceImpl> impl;
    MockResourceGuard guard(&impl);

    cfg.fast_free.resource = &resource;
    cfg.threading.resource = &resource;
    cfg.large_alloc.resource = &resource;
    cfg.chunk_locator.resource = &resource;
    cfg.reuse.resource = &resource;
    cfg.freelist.resource = &resource;
    cfg.chunk_size = 256;
    cfg.min_block_size = 64;
    cfg.max_block_size = 128;
    cfg.freelist.min_block_size = cfg.min_block_size;
    cfg.freelist.max_block_size = cfg.max_block_size;
    pool.initialize(cfg);

    void* base = reinterpret_cast<void*>(0x4000);
    EXPECT_CALL(impl, allocate(300, 16)).WillOnce(Return(CpuBufferView{base, 0, 300}));
    EXPECT_CALL(impl, deallocate(testing::_, 300, 16)).Times(1);

    Pool::LaunchParams params{};
    MemoryBlock block = pool.allocate(300, 16, params);
    ASSERT_TRUE(block.valid());

    pool.deallocate(block, 300, 16, params);
}

}  // namespace

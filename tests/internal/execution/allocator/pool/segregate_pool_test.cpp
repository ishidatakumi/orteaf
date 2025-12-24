#include "orteaf/internal/execution/allocator/pool/segregate_pool.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/execution/allocator/buffer_resource.h"
#include "orteaf/internal/execution/allocator/policies/chunk_locator/direct_chunk_locator.h"
#include "orteaf/internal/execution/allocator/policies/fast_free/fast_free_policies.h"
#include "orteaf/internal/execution/allocator/policies/freelist/host_stack_freelist_policy.h"
#include "orteaf/internal/execution/allocator/policies/large_alloc/direct_resource_large_alloc.h"
#include "orteaf/internal/execution/allocator/policies/reuse/deferred_reuse_policy.h"
#include "orteaf/internal/execution/allocator/policies/threading/threading_policies.h"
#include "orteaf/internal/execution/cpu/resource/cpu_buffer_view.h"
#include "tests/internal/execution/allocator/testing/mock_resource.h"

namespace {

using ::orteaf::internal::backend::Backend;
using ::orteaf::internal::execution::allocator::BufferBlock;
using ::orteaf::internal::execution::allocator::BufferResource;
using ::orteaf::internal::execution::allocator::testing::MockCpuResource;
using ::orteaf::internal::execution::allocator::testing::MockCpuResourceImpl;
namespace policies = ::orteaf::internal::execution::allocator::policies;
using CpuBufferView = ::orteaf::internal::execution::cpu::resource::CpuBufferView;
using ::testing::NiceMock;
using ::testing::Return;

// ---------------------------------------------------------------------------
// Test doubles
// ---------------------------------------------------------------------------
struct MockResource {
  using BufferView = CpuBufferView;
  using BufferResource =
      ::orteaf::internal::execution::allocator::BufferResource<Backend::Cpu>;
  using BufferBlock =
      ::orteaf::internal::execution::allocator::BufferBlock<Backend::Cpu>;
  using FenceToken = typename BufferResource::FenceToken;
  struct ReuseToken {
    ReuseToken() = default;
    explicit ReuseToken(FenceToken &&) {}
  };
  struct LaunchParams {};

  static constexpr Backend backend_type_static() noexcept {
    return Backend::Cpu;
  }
  constexpr Backend backend_type() const noexcept {
    return backend_type_static();
  }

  struct Config {
    // Empty config for mock resource
  };

  void initialize(const Config &) {
    // No-op for mock resource
  }

  static void set(MockCpuResourceImpl *impl) { MockCpuResource::set(impl); }
  static void reset() { MockCpuResource::reset(); }

  static BufferView allocate(std::size_t size, std::size_t alignment) {
    return MockCpuResource::allocate(size, alignment);
  }

  static void deallocate(BufferView view, std::size_t size,
                         std::size_t alignment) {
    MockCpuResource::deallocate(view, size, alignment);
  }

  static BufferView makeView(BufferView base, std::size_t offset,
                             std::size_t size) {
    return MockCpuResource::makeView(base, offset, size);
  }

  static bool isCompleted(ReuseToken &) { return true; }
};

struct MockResourceGuard {
  explicit MockResourceGuard(MockCpuResourceImpl *impl) {
    MockResource::set(impl);
  }
  ~MockResourceGuard() { MockResource::reset(); }
};

using Pool = ::orteaf::internal::execution::allocator::pool::SegregatePool<
    MockResource, policies::FastFreePolicy, policies::NoLockThreadingPolicy,
    policies::DirectResourceLargeAllocPolicy<MockResource>,
    policies::DirectChunkLocatorPolicy<MockResource>,
    policies::DeferredReusePolicy<MockResource>,
    policies::HostStackFreelistPolicy<MockResource>>;

using BufferResourceType = BufferResource<Backend::Cpu>;

TEST(SegregatePool, InitializePropagatesToAllPolicies) {
  NiceMock<MockCpuResourceImpl> impl;
  MockResourceGuard guard(&impl);

  Pool pool(MockResource{});
  Pool::Config cfg{};

  cfg.fast_free.resource = pool.resource();
  cfg.threading.resource = pool.resource();
  cfg.large_alloc.resource = pool.resource();
  cfg.chunk_locator.resource = pool.resource();
  cfg.reuse.resource = pool.resource();
  cfg.freelist.resource = pool.resource();
  cfg.min_block_size = 64;
  cfg.max_block_size = 256;

  EXPECT_CALL(impl, allocate).Times(0);

  pool.initialize(cfg);

  EXPECT_EQ(pool.free_list_policy().get_active_freelist_count(), 1u);
  EXPECT_TRUE(pool.free_list_policy().empty(0));
}

TEST(SegregatePool, AllocatesFromChunkWhenBelowMaxSize) {
  NiceMock<MockCpuResourceImpl> impl;
  MockResourceGuard guard(&impl);
  ON_CALL(impl, makeView)
      .WillByDefault(
          [](CpuBufferView base, std::size_t offset, std::size_t size) {
            return CpuBufferView{base.raw(), offset, size};
          });

  Pool pool(MockResource{});
  Pool::Config cfg{};

  cfg.fast_free.resource = pool.resource();
  cfg.threading.resource = pool.resource();
  cfg.large_alloc.resource = pool.resource();
  cfg.chunk_locator.resource = pool.resource();
  cfg.reuse.resource = pool.resource();
  cfg.freelist.resource = pool.resource();
  cfg.chunk_size = 256;
  cfg.min_block_size = 64;
  cfg.max_block_size = 256;
  pool.initialize(cfg);

  // One chunk allocation; freelist should hand back a block view aligned to
  // block_size.
  void *base = reinterpret_cast<void *>(0x1000);
  const std::size_t block_size = 128; // ceil(max(64, 80))
  EXPECT_CALL(impl, allocate(256, 0))
      .WillOnce(Return(CpuBufferView{base, 0, 256}));

  Pool::LaunchParams params{};
  BufferResourceType block = pool.allocate(80, 64, params);

  EXPECT_TRUE(block.valid());
  EXPECT_EQ(block.view.size(), block_size);
  EXPECT_LT(block.view.offset(), cfg.chunk_size);
}

TEST(SegregatePool, UsesLargeAllocWhenOverMaxSize) {
  NiceMock<MockCpuResourceImpl> impl;
  MockResourceGuard guard(&impl);
  ON_CALL(impl, makeView)
      .WillByDefault(
          [](CpuBufferView base, std::size_t offset, std::size_t size) {
            return CpuBufferView{base.raw(), offset, size};
          });

  Pool pool(MockResource{});
  Pool::Config cfg{};

  cfg.fast_free.resource = pool.resource();
  cfg.threading.resource = pool.resource();
  cfg.large_alloc.resource = pool.resource();
  cfg.chunk_locator.resource = pool.resource();
  cfg.reuse.resource = pool.resource();
  cfg.freelist.resource = pool.resource();
  cfg.chunk_size = 256;
  cfg.min_block_size = 64;
  cfg.max_block_size = 128;
  pool.initialize(cfg);

  void *base = reinterpret_cast<void *>(0x2000);
  EXPECT_CALL(impl, allocate(300, 16))
      .WillOnce(Return(CpuBufferView{base, 0, 300}));

  Pool::LaunchParams params{};
  BufferResourceType block = pool.allocate(300, 16, params);

  EXPECT_TRUE(block.valid());
  EXPECT_EQ(block.view.size(), 300u);
  EXPECT_EQ(block.view.offset(), 0u);
}

TEST(SegregatePool, DeallocateReturnsBlockToFreelist) {
  NiceMock<MockCpuResourceImpl> impl;
  MockResourceGuard guard(&impl);
  ON_CALL(impl, makeView)
      .WillByDefault(
          [](CpuBufferView base, std::size_t offset, std::size_t size) {
            return CpuBufferView{base.raw(), offset, size};
          });

  Pool pool(MockResource{});
  Pool::Config cfg{};

  cfg.fast_free.resource = pool.resource();
  cfg.threading.resource = pool.resource();
  cfg.large_alloc.resource = pool.resource();
  cfg.chunk_locator.resource = pool.resource();
  cfg.reuse.resource = pool.resource();
  cfg.freelist.resource = pool.resource();
  cfg.chunk_size = 256;
  cfg.min_block_size = 64;
  cfg.max_block_size = 256;
  pool.initialize(cfg);

  void *base = reinterpret_cast<void *>(0x3000);
  EXPECT_CALL(impl, allocate(256, 0))
      .WillOnce(Return(CpuBufferView{base, 0, 256}));

  Pool::LaunchParams params{};
  BufferResourceType block = pool.allocate(80, 64, params);
  ASSERT_TRUE(block.valid());
  testing::Mock::VerifyAndClearExpectations(&impl);

  pool.deallocate(block, 80, 64, params);

  EXPECT_CALL(impl, allocate).Times(0);
  BufferResourceType reused = pool.allocate(80, 64, params);
  EXPECT_TRUE(reused.valid());
  EXPECT_EQ(reused.view.size(), block.view.size());
}

TEST(SegregatePool, DeallocateLargeAllocUsesLargePolicy) {
  NiceMock<MockCpuResourceImpl> impl;
  MockResourceGuard guard(&impl);

  Pool pool(MockResource{});
  Pool::Config cfg{};

  cfg.fast_free.resource = pool.resource();
  cfg.threading.resource = pool.resource();
  cfg.large_alloc.resource = pool.resource();
  cfg.chunk_locator.resource = pool.resource();
  cfg.reuse.resource = pool.resource();
  cfg.freelist.resource = pool.resource();
  cfg.chunk_size = 256;
  cfg.min_block_size = 64;
  cfg.max_block_size = 128;
  pool.initialize(cfg);

  void *base = reinterpret_cast<void *>(0x4000);
  EXPECT_CALL(impl, allocate(300, 16))
      .WillOnce(Return(CpuBufferView{base, 0, 300}));
  EXPECT_CALL(impl, deallocate(testing::_, 300, 16)).Times(1);

  Pool::LaunchParams params{};
  BufferResourceType block = pool.allocate(300, 16, params);
  ASSERT_TRUE(block.valid());

  pool.deallocate(block, 300, 16, params);
}

TEST(SegregatePool, ReleaseChunkFreesBackingAndForcesNewChunkOnNextAlloc) {
  NiceMock<MockCpuResourceImpl> impl;
  MockResourceGuard guard(&impl);
  ON_CALL(impl, makeView)
      .WillByDefault(
          [](CpuBufferView base, std::size_t offset, std::size_t size) {
            return CpuBufferView{base.raw(), offset, size};
          });

  Pool pool(MockResource{});
  Pool::Config cfg{};

  cfg.fast_free.resource = pool.resource();
  cfg.threading.resource = pool.resource();
  cfg.large_alloc.resource = pool.resource();
  cfg.chunk_locator.resource = pool.resource();
  cfg.reuse.resource = pool.resource();
  cfg.freelist.resource = pool.resource();
  cfg.chunk_size = 256;
  cfg.min_block_size = 64;
  cfg.max_block_size = 256;
  pool.initialize(cfg);

  void *base = reinterpret_cast<void *>(0x5000);
  void *base2 = reinterpret_cast<void *>(0x6000);
  EXPECT_CALL(impl, allocate(256, 0))
      .WillOnce(Return(CpuBufferView{base, 0, 256}))
      .WillOnce(Return(CpuBufferView{base2, 0, 256}));
  EXPECT_CALL(impl, deallocate(testing::_, 256, 0)).Times(1);

  Pool::LaunchParams params{};
  BufferResourceType block = pool.allocate(80, 64, params);
  ASSERT_TRUE(block.valid());

  pool.deallocate(block, 80, 64, params);
  pool.releaseChunk(params);

  BufferResourceType new_block = pool.allocate(80, 64, params);
  EXPECT_TRUE(new_block.valid());
  EXPECT_EQ(new_block.view.raw(), base2);
}

TEST(SegregatePool, StatsTracking) {
  NiceMock<MockCpuResourceImpl> impl;
  MockResourceGuard guard(&impl);
  ON_CALL(impl, makeView)
      .WillByDefault(
          [](CpuBufferView base, std::size_t offset, std::size_t size) {
            return CpuBufferView{base.raw(), offset, size};
          });

  Pool pool(MockResource{});
  Pool::Config cfg{};

  cfg.fast_free.resource = pool.resource();
  cfg.threading.resource = pool.resource();
  cfg.large_alloc.resource = pool.resource();
  cfg.chunk_locator.resource = pool.resource();
  cfg.reuse.resource = pool.resource();
  cfg.freelist.resource = pool.resource();
  cfg.chunk_size = 256;
  cfg.min_block_size = 64;
  cfg.max_block_size = 256;
  pool.initialize(cfg);

  const auto &stats = pool.stats();
  // Verify initial state
  EXPECT_EQ(stats.totalAllocations(), 0u);
  EXPECT_EQ(stats.activeAllocations(), 0u);
  EXPECT_EQ(stats.poolExpansions(), 0u);

  // Mock allocation for expansion
  void *base = reinterpret_cast<void *>(0x7000);
  EXPECT_CALL(impl, allocate(256, 0))
      .WillOnce(Return(CpuBufferView{base, 0, 256}));

  Pool::LaunchParams params{};
  // Allocation triggers expansion
  BufferResourceType block = pool.allocate(64, 64, params);
  ASSERT_TRUE(block.valid());

  std::string stats_str = stats.toString();
  if (stats_str.find("Disabled") == std::string::npos) {
    // Stats are enabled
    EXPECT_EQ(stats.totalAllocations(), 1u);
    EXPECT_EQ(stats.activeAllocations(), 1u);
    EXPECT_EQ(stats.poolExpansions(), 1u);

    // Deallocation
    pool.deallocate(block, 64, 64, params);
    EXPECT_EQ(stats.totalAllocations(), 1u);
    EXPECT_EQ(stats.totalDeallocations(), 1u);
    EXPECT_EQ(stats.activeAllocations(), 0u);
  } else {
    // Stats are disabled
    EXPECT_EQ(stats.totalAllocations(), 0u);
    EXPECT_EQ(stats.activeAllocations(), 0u);
  }
}

} // namespace

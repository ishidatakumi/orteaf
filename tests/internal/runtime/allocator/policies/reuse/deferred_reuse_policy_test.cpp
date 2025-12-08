#include "orteaf/internal/runtime/allocator/policies/reuse/deferred_reuse_policy.h"

#include <atomic>
#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/runtime/allocator/memory_block.h"
#include "orteaf/internal/runtime/base/backend_traits.h"
#include "orteaf/internal/runtime/cpu/resource/cpu_buffer_view.h"
#include "tests/internal/testing/error_assert.h"

namespace allocator = ::orteaf::internal::runtime::allocator;
namespace policies = ::orteaf::internal::runtime::allocator::policies;
using Backend = ::orteaf::internal::backend::Backend;
using BufferHandle = ::orteaf::internal::base::BufferHandle;
using CpuView = ::orteaf::internal::runtime::cpu::resource::CpuBufferView;
namespace {
using MemoryBlock = allocator::MemoryBlock<Backend::Cpu>;
using CpuReuseToken =
    ::orteaf::internal::runtime::base::BackendTraits<Backend::Cpu>::ReuseToken;
struct FakeResource;
using Policy = policies::DeferredReusePolicy<FakeResource, Backend::Cpu>;

struct FakeResource {
  std::atomic<bool> next_result{true};
  std::atomic<int> calls{0};

  bool isCompleted(const CpuReuseToken & /*token*/) {
    ++calls;
    return next_result.load();
  }
};

MemoryBlock makeBlock(BufferHandle id,
                      void *ptr = reinterpret_cast<void *>(0x10),
                      std::size_t size = 64) {
  return MemoryBlock{id, CpuView{ptr, 0, size}};
}

TEST(DeferredReusePolicy, InitializeFailsWithNullResource) {
  Policy policy;
  Policy::Config cfg{};

  orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [&] { policy.initialize(nullptr, cfg); });
}

TEST(DeferredReusePolicy, MovesCompletedToReady) {
  FakeResource resource;
  Policy policy;
  policy.initialize(&resource);

  MemoryBlock block = makeBlock(BufferHandle{1});
  std::size_t freelist_index = 3;
  CpuReuseToken token{};

  policy.scheduleForReuse(block, freelist_index, token);
  EXPECT_EQ(policy.processPending(), 1u);

  MemoryBlock out_block{};
  std::size_t out_index = 0;
  EXPECT_TRUE(policy.getReadyItem(out_block, out_index));
  EXPECT_EQ(out_block.handle, block.handle);
  EXPECT_EQ(out_index, freelist_index);
}

TEST(DeferredReusePolicy, KeepsPendingWhenNotCompleted) {
  FakeResource resource;
  resource.next_result.store(false);
  Policy policy;
  policy.initialize(&resource);

  MemoryBlock block = makeBlock(BufferHandle{2});
  CpuReuseToken token{};
  policy.scheduleForReuse(block, 1, token);

  EXPECT_EQ(policy.processPending(), 0u);
  EXPECT_TRUE(policy.hasPending());
  EXPECT_EQ(policy.getPendingReuseCount(), 1u);

  resource.next_result.store(true);
  EXPECT_EQ(policy.processPending(), 1u);
  EXPECT_FALSE(policy.hasPending());
}

TEST(DeferredReusePolicy, RemoveBlocksInChunkFiltersPendingAndReady) {
  FakeResource resource;
  Policy policy;
  policy.initialize(&resource);

  CpuReuseToken token{};
  MemoryBlock block1 = makeBlock(BufferHandle{10});
  MemoryBlock block2 =
      makeBlock(BufferHandle{20}, reinterpret_cast<void *>(0x20));

  policy.scheduleForReuse(block1, 0, token);
  resource.next_result.store(true);
  EXPECT_EQ(policy.processPending(), 1u);

  resource.next_result.store(false);
  policy.scheduleForReuse(block2, 1, token);
  EXPECT_TRUE(policy.hasPending());

  policy.removeBlocksInChunk(block1.handle);
  policy.removeBlocksInChunk(block2.handle);

  MemoryBlock out_block{};
  std::size_t out_index = 0;
  EXPECT_FALSE(policy.getReadyItem(out_block, out_index));
  EXPECT_EQ(policy.getPendingReuseCount(), 0u);
}

TEST(DeferredReusePolicy, FlushPendingWaitsUntilComplete) {
  FakeResource resource;
  Policy policy;
  policy.initialize(&resource, Policy::Config{std::chrono::milliseconds{5}});

  CpuReuseToken token{};
  resource.next_result.store(false);
  policy.scheduleForReuse(makeBlock(BufferHandle{30}), 2, token);

  // Flip to complete after a short delay in another thread.
  std::thread toggler([&resource] {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    resource.next_result.store(true);
  });

  policy.flushPending();
  toggler.join();

  MemoryBlock out_block{};
  std::size_t out_index = 0;
  EXPECT_TRUE(policy.getReadyItem(out_block, out_index));
  EXPECT_FALSE(policy.hasPending());
}

} // namespace

#include "orteaf/internal/execution/allocator/policies/reuse/deferred_reuse_policy.h"

#include <atomic>
#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/execution/allocator/buffer_resource.h"
#include "orteaf/internal/execution/cpu/resource/cpu_buffer_view.h"
#include "tests/internal/testing/error_assert.h"

namespace allocator = ::orteaf::internal::execution::allocator;
namespace policies = ::orteaf::internal::execution::allocator::policies;
using Backend = ::orteaf::internal::backend::Backend;
using BufferViewHandle = ::orteaf::internal::base::BufferViewHandle;
using CpuView = ::orteaf::internal::execution::cpu::resource::CpuBufferView;
namespace {
using BufferResource = allocator::BufferResource<Backend::Cpu>;
using BufferBlock = allocator::BufferBlock<Backend::Cpu>;
using FenceToken = typename BufferResource::FenceToken;

struct FakeResource;
using Policy = policies::DeferredReusePolicy<FakeResource>;

struct FakeResource {
  using BufferResource = allocator::BufferResource<Backend::Cpu>;
  struct ReuseToken {
    ReuseToken() = default;
    explicit ReuseToken(FenceToken &&) {}
  };

  static constexpr Backend backend_type_static() noexcept {
    return Backend::Cpu;
  }

  std::atomic<bool> next_result{true};
  std::atomic<int> calls{0};

  bool isCompleted(ReuseToken & /*token*/) {
    ++calls;
    return next_result.load();
  }
};

BufferResource makeBlock(BufferViewHandle id,
                         void *ptr = reinterpret_cast<void *>(0x10),
                         std::size_t size = 64) {
  return BufferResource{id, CpuView{ptr, 0, size}};
}

TEST(DeferredReusePolicy, InitializeFailsWithNullResource) {
  Policy policy;
  Policy::Config cfg{};

  orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [&] { policy.initialize(cfg); });
}

TEST(DeferredReusePolicy, MovesCompletedToReady) {
  FakeResource resource;
  Policy policy;
  Policy::Config cfg{};
  cfg.resource = &resource;
  policy.initialize(cfg);

  BufferResource block = makeBlock(BufferViewHandle{1});
  std::size_t freelist_index = 3;

  policy.scheduleForReuse(std::move(block), freelist_index);
  EXPECT_EQ(policy.processPending(), 1u);

  BufferBlock out_block{};
  std::size_t out_index = 0;
  EXPECT_TRUE(policy.getReadyItem(out_index, out_block));
  EXPECT_EQ(out_block.handle, BufferViewHandle{1});
  EXPECT_EQ(out_index, freelist_index);
}

TEST(DeferredReusePolicy, KeepsPendingWhenNotCompleted) {
  FakeResource resource;
  resource.next_result.store(false);
  Policy policy;
  Policy::Config cfg{};
  cfg.resource = &resource;
  policy.initialize(cfg);

  BufferResource block = makeBlock(BufferViewHandle{2});
  policy.scheduleForReuse(std::move(block), 1);

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
  Policy::Config cfg{};
  cfg.resource = &resource;
  policy.initialize(cfg);

  BufferResource block1 = makeBlock(BufferViewHandle{10});
  BufferResource block2 =
      makeBlock(BufferViewHandle{20}, reinterpret_cast<void *>(0x20));

  policy.scheduleForReuse(std::move(block1), 0);
  resource.next_result.store(true);
  EXPECT_EQ(policy.processPending(), 1u);

  resource.next_result.store(false);
  policy.scheduleForReuse(std::move(block2), 1);
  EXPECT_TRUE(policy.hasPending());

  policy.removeBlocksInChunk(BufferViewHandle{10});
  policy.removeBlocksInChunk(BufferViewHandle{20});

  BufferBlock out_block{};
  std::size_t out_index = 0;
  EXPECT_FALSE(policy.getReadyItem(out_index, out_block));
  EXPECT_EQ(policy.getPendingReuseCount(), 0u);
}

TEST(DeferredReusePolicy, FlushPendingWaitsUntilComplete) {

  FakeResource resource;
  Policy policy;
  Policy::Config cfg;
  cfg.resource = &resource;
  cfg.timeout_ms = std::chrono::milliseconds{5};
  policy.initialize(cfg);

  resource.next_result.store(false);
  policy.scheduleForReuse(makeBlock(BufferViewHandle{30}), 2);

  // Flip to complete after a short delay in another thread.
  std::thread toggler([&resource] {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    resource.next_result.store(true);
  });

  policy.flushPending();
  toggler.join();

  BufferBlock out_block{};
  std::size_t out_index = 0;
  EXPECT_TRUE(policy.getReadyItem(out_index, out_block));
  EXPECT_FALSE(policy.hasPending());
}

} // namespace

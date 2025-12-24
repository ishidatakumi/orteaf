#include "orteaf/internal/execution/allocator/policies/freelist/host_stack_freelist_policy.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "orteaf/internal/backend/backend.h"
#include "tests/internal/execution/allocator/testing/mock_resource.h"

using ::orteaf::internal::backend::Backend;
using ::orteaf::internal::base::BufferViewHandle;
using ::orteaf::internal::execution::allocator::testing::MockCpuResource;
using ::orteaf::internal::execution::allocator::testing::MockCpuResourceImpl;
using ::orteaf::internal::execution::cpu::resource::CpuBufferView;
namespace policies = ::orteaf::internal::execution::allocator::policies;

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Sequence;

namespace {

using Policy = policies::HostStackFreelistPolicy<MockCpuResource>;
using BufferBlock = Policy::BufferBlock;

TEST(HostStackFreelistPolicy, ConfigureInitializesStacks) {
  Policy policy;
  MockCpuResource resource;

  Policy::Config cfg{};
  cfg.resource = &resource;
  // サイズクラス数を直接渡す (log2(256/64) + 1 = 3)
  policy.initialize(cfg, 3);

  EXPECT_EQ(policy.get_active_freelist_count(), 1u);
  EXPECT_EQ(policy.get_total_free_blocks(), 0u);
  EXPECT_TRUE(policy.empty(0));
  EXPECT_TRUE(policy.empty(2));
}

TEST(HostStackFreelistPolicy, PushAndPopAreLifoAndResizeStacks) {
  Policy policy;
  MockCpuResource resource;
  Policy::Config cfg{};
  cfg.resource = &resource;
  // サイズクラス数を直接渡す (log2(128/64) + 1 = 2)
  policy.initialize(cfg, 2);

  BufferBlock first{BufferViewHandle{1},
                    CpuBufferView{reinterpret_cast<void *>(0x1), 0, 64}};
  BufferBlock second{BufferViewHandle{2},
                     CpuBufferView{reinterpret_cast<void *>(0x1), 64, 64}};

  policy.push(2, first);
  policy.push(2, second);

  auto popped_first = policy.pop(2);
  EXPECT_EQ(popped_first.handle, BufferViewHandle{2});
  EXPECT_EQ(popped_first.view.offset(), 64u);

  auto popped_second = policy.pop(2);
  EXPECT_EQ(popped_second.handle, BufferViewHandle{1});
  EXPECT_TRUE(policy.empty(2));
}

TEST(HostStackFreelistPolicy, ExpandSplitsChunkIntoBlocks) {

  Policy policy;
  MockCpuResource resource;
  Policy::Config cfg{};
  cfg.resource = &resource;
  // サイズクラス数を渡す (必要に応じて動的にリサイズされる)
  policy.initialize(cfg, 3);

  NiceMock<MockCpuResourceImpl> impl;
  MockCpuResource::set(&impl);

  void *base = reinterpret_cast<void *>(0x1000);
  CpuBufferView chunk_view{base, 0, 256};
  BufferBlock chunk{BufferViewHandle{3}, chunk_view};

  Sequence seq;
  EXPECT_CALL(impl, makeView(chunk_view, 0, 64))
      .InSequence(seq)
      .WillOnce(Return(CpuBufferView{base, 0, 64}));
  EXPECT_CALL(impl, makeView(chunk_view, 64, 64))
      .InSequence(seq)
      .WillOnce(Return(CpuBufferView{base, 64, 64}));
  EXPECT_CALL(impl, makeView(chunk_view, 128, 64))
      .InSequence(seq)
      .WillOnce(Return(CpuBufferView{base, 128, 64}));
  EXPECT_CALL(impl, makeView(chunk_view, 192, 64))
      .InSequence(seq)
      .WillOnce(Return(CpuBufferView{base, 192, 64}));

  policy.expand(0, chunk, 256, 64);
  EXPECT_EQ(policy.get_total_free_blocks(), 4u);

  EXPECT_EQ(policy.pop(0).view.offset(), 192u);
  EXPECT_EQ(policy.pop(0).view.offset(), 128u);
  EXPECT_EQ(policy.pop(0).view.offset(), 64u);
  EXPECT_EQ(policy.pop(0).view.offset(), 0u);
  EXPECT_TRUE(policy.empty(0));

  MockCpuResource::reset();
}

TEST(HostStackFreelistPolicy, RemoveBlocksInChunkRemovesContainedBlocks) {
  Policy policy;
  MockCpuResource resource;
  Policy::Config cfg{};
  cfg.resource = &resource;
  policy.initialize(cfg, 2);

  NiceMock<MockCpuResourceImpl> impl;
  MockCpuResource::set(&impl);

  void *base = reinterpret_cast<void *>(0x2000);
  CpuBufferView chunk_view{base, 0, 128};
  BufferBlock chunk{BufferViewHandle{4}, chunk_view};

  Sequence seq;
  EXPECT_CALL(impl, makeView(chunk_view, 0, 32))
      .InSequence(seq)
      .WillOnce(Return(CpuBufferView{base, 0, 32}));
  EXPECT_CALL(impl, makeView(chunk_view, 32, 32))
      .InSequence(seq)
      .WillOnce(Return(CpuBufferView{base, 32, 32}));
  EXPECT_CALL(impl, makeView(chunk_view, 64, 32))
      .InSequence(seq)
      .WillOnce(Return(CpuBufferView{base, 64, 32}));
  EXPECT_CALL(impl, makeView(chunk_view, 96, 32))
      .InSequence(seq)
      .WillOnce(Return(CpuBufferView{base, 96, 32}));

  policy.expand(0, chunk, 128, 32);
  BufferBlock other{BufferViewHandle{99},
                    CpuBufferView{reinterpret_cast<void *>(0xDEADBEEF), 0, 32}};
  policy.push(0, other);
  EXPECT_EQ(policy.get_total_free_blocks(), 5u);

  policy.removeBlocksInChunk(chunk.handle);
  EXPECT_EQ(policy.get_total_free_blocks(), 1u);

  auto remaining = policy.pop(0);
  EXPECT_EQ(remaining.handle, BufferViewHandle{99});
  EXPECT_FALSE(remaining.view.empty());
  EXPECT_TRUE(policy.empty(0));

  MockCpuResource::reset();
}

} // namespace

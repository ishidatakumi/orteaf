#include "orteaf/internal/runtime/allocator/policies/freelist/host_stack_freelist_policy.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "orteaf/internal/backend/backend.h"
#include "tests/internal/runtime/allocator/testing/mock_resource.h"

using ::orteaf::internal::backend::Backend;
using ::orteaf::internal::base::BufferHandle;
using ::orteaf::internal::runtime::allocator::testing::MockCpuResource;
using ::orteaf::internal::runtime::allocator::testing::MockCpuResourceImpl;
using ::orteaf::internal::runtime::cpu::resource::CpuBufferView;
namespace policies = ::orteaf::internal::runtime::allocator::policies;

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Sequence;

namespace {

using Policy = policies::HostStackFreelistPolicy<MockCpuResource, Backend::Cpu>;
using MemoryBlock = Policy::MemoryBlock;

TEST(HostStackFreelistPolicy, ConfigureInitializesStacks) {
  Policy policy;
  MockCpuResource resource;

  policy.initialize(&resource);
  policy.configureBounds(64, 256);

  EXPECT_EQ(policy.get_active_freelist_count(), 1u);
  EXPECT_EQ(policy.get_total_free_blocks(), 0u);
  EXPECT_TRUE(policy.empty(0));
  EXPECT_TRUE(policy.empty(2));
}

TEST(HostStackFreelistPolicy, PushAndPopAreLifoAndResizeStacks) {
  Policy policy;
  MockCpuResource resource;
  policy.initialize(&resource);
  policy.configureBounds(64, 128);

  MemoryBlock first{BufferHandle{1},
                    CpuBufferView{reinterpret_cast<void *>(0x1), 0, 64}};
  MemoryBlock second{BufferHandle{2},
                     CpuBufferView{reinterpret_cast<void *>(0x1), 64, 64}};

  policy.push(2, first);
  policy.push(2, second);

  auto popped_first = policy.pop(2);
  EXPECT_EQ(popped_first.handle, BufferHandle{2});
  EXPECT_EQ(popped_first.view.offset(), 64u);

  auto popped_second = policy.pop(2);
  EXPECT_EQ(popped_second.handle, BufferHandle{1});
  EXPECT_TRUE(policy.empty(2));
}

TEST(HostStackFreelistPolicy, ExpandSplitsChunkIntoBlocks) {
  Policy policy;
  MockCpuResource resource;
  policy.initialize(&resource);

  NiceMock<MockCpuResourceImpl> impl;
  MockCpuResource::set(&impl);

  void *base = reinterpret_cast<void *>(0x1000);
  CpuBufferView chunk_view{base, 0, 256};
  MemoryBlock chunk{BufferHandle{3}, chunk_view};

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
  policy.initialize(&resource);

  NiceMock<MockCpuResourceImpl> impl;
  MockCpuResource::set(&impl);

  void *base = reinterpret_cast<void *>(0x2000);
  CpuBufferView chunk_view{base, 0, 128};
  MemoryBlock chunk{BufferHandle{4}, chunk_view};

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
  MemoryBlock other{BufferHandle{99},
                    CpuBufferView{reinterpret_cast<void *>(0xDEADBEEF), 0, 32}};
  policy.push(0, other);
  EXPECT_EQ(policy.get_total_free_blocks(), 5u);

  policy.removeBlocksInChunk(chunk, 128);
  EXPECT_EQ(policy.get_total_free_blocks(), 1u);

  auto remaining = policy.pop(0);
  EXPECT_EQ(remaining.handle, BufferHandle{99});
  EXPECT_FALSE(remaining.view.empty());
  EXPECT_TRUE(policy.empty(0));

  MockCpuResource::reset();
}

} // namespace

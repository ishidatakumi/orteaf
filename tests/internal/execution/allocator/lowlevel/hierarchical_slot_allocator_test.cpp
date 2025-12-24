#include "orteaf/internal/execution/allocator/lowlevel/hierarchical_slot_allocator.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/execution/base/backend_traits.h"
#include "tests/internal/execution/allocator/testing/mock_resource.h"
#include "tests/internal/testing/error_assert.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace allocator = ::orteaf::internal::execution::allocator;
namespace policies = ::orteaf::internal::execution::allocator::policies;
using Backend = ::orteaf::internal::backend::Backend;
using Traits = ::orteaf::internal::execution::base::BackendTraits<Backend::Cpu>;
using BufferView = Traits::BufferView;
using HeapRegion = Traits::HeapRegion;
using ::orteaf::internal::execution::allocator::testing::MockCpuHeapOps;
using ::orteaf::internal::execution::allocator::testing::MockCpuHeapOpsImpl;
using OrteafErrc = ::orteaf::internal::diagnostics::error::OrteafErrc;

namespace {

using Allocator =
    policies::HierarchicalSlotAllocator<MockCpuHeapOps, Backend::Cpu>;

static BufferView MapReturn(HeapRegion region) {
  return BufferView{region.data(), 0, region.size()};
}

class HierarchicalSlotAllocatorTest : public ::testing::Test {
protected:
  void SetUp() override { MockCpuHeapOps::set(&impl_); }

  void TearDown() override { MockCpuHeapOps::reset(); }

  NiceMock<MockCpuHeapOpsImpl> impl_;
  MockCpuHeapOps heap_ops_;
  Allocator allocator_;
};

TEST_F(HierarchicalSlotAllocatorTest, AllocateReturnsBufferView) {
  void *base = reinterpret_cast<void *>(0x1000);
  EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base, 256}));
  EXPECT_CALL(impl_, map(_)).WillOnce(::testing::Invoke(MapReturn));

  Allocator::Config cfg{};
  cfg.levels = {256};
  allocator_.initialize(cfg, &heap_ops_);

  auto view = allocator_.allocate(256);
  EXPECT_TRUE(view);
  EXPECT_EQ(view.data(), base);
  EXPECT_EQ(view.size(), 256);
}

TEST_F(HierarchicalSlotAllocatorTest, DeallocateCallsUnmap) {
  void *base = reinterpret_cast<void *>(0x2000);
  EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base, 256}));
  EXPECT_CALL(impl_, map(_)).WillOnce(::testing::Invoke(MapReturn));
  EXPECT_CALL(impl_, unmap(_, 256)).Times(1);

  Allocator::Config cfg{};
  cfg.levels = {256};
  allocator_.initialize(cfg, &heap_ops_);

  auto view = allocator_.allocate(256);
  allocator_.deallocate(view);
}

TEST_F(HierarchicalSlotAllocatorTest, AllocateSmallSizeFromLargerSlot) {
  // levels = {256, 128} で 128 バイトを要求
  // 256 バイトのスロットから 128 バイトのスロットに分割されるはず
  void *base = reinterpret_cast<void *>(0x3000);
  EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base, 256}));
  EXPECT_CALL(impl_, map(_)).WillOnce(::testing::Invoke(MapReturn));

  Allocator::Config cfg{};
  cfg.levels = {256, 128};
  cfg.initial_bytes = 256;
  allocator_.initialize(cfg, &heap_ops_);

  auto view = allocator_.allocate(128);
  EXPECT_TRUE(view);
  EXPECT_EQ(view.size(), 128);
}

TEST_F(HierarchicalSlotAllocatorTest, DeallocateSplitSlotCallsUnmap) {
  // levels = {256, 128} で 128 バイトを確保して解放
  void *base = reinterpret_cast<void *>(0x4000);
  EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base, 256}));
  EXPECT_CALL(impl_, map(_)).WillOnce(::testing::Invoke(MapReturn));
  EXPECT_CALL(impl_, unmap(_, 128)).Times(1);

  Allocator::Config cfg{};
  cfg.levels = {256, 128};
  cfg.initial_bytes = 256;
  allocator_.initialize(cfg, &heap_ops_);

  auto view = allocator_.allocate(128);
  EXPECT_TRUE(view);
  EXPECT_EQ(view.size(), 128);

  allocator_.deallocate(view);
}

// ============================================================================
// Validation tests
// ============================================================================

TEST_F(HierarchicalSlotAllocatorTest, InitializeThrowsOnEmptyLevels) {
  Allocator::Config cfg{};
  cfg.levels = {};
  orteaf::tests::ExpectError(OrteafErrc::InvalidParameter,
                             [&] { allocator_.initialize(cfg, &heap_ops_); });
}

TEST_F(HierarchicalSlotAllocatorTest, InitializeThrowsOnInvalidInitialBytes) {
  Allocator::Config cfg{};
  cfg.levels = {256};
  cfg.initial_bytes = 100; // 256の倍数ではない
  orteaf::tests::ExpectError(OrteafErrc::InvalidParameter,
                             [&] { allocator_.initialize(cfg, &heap_ops_); });
}

TEST_F(HierarchicalSlotAllocatorTest, InitializeAcceptsValidInitialBytes) {
  void *base = reinterpret_cast<void *>(0x5000);
  EXPECT_CALL(impl_, reserve(512)).WillOnce(Return(HeapRegion{base, 512}));

  Allocator::Config cfg{};
  cfg.levels = {256};
  cfg.initial_bytes = 512; // 256の倍数
  EXPECT_NO_THROW(allocator_.initialize(cfg, &heap_ops_));
}

TEST_F(HierarchicalSlotAllocatorTest, InitializeThrowsOnZeroLevel) {
  Allocator::Config cfg{};
  cfg.levels = {256, 0, 64};
  orteaf::tests::ExpectError(OrteafErrc::InvalidParameter,
                             [&] { allocator_.initialize(cfg, &heap_ops_); });
}

TEST_F(HierarchicalSlotAllocatorTest, InitializeThrowsOnNonDecreasingLevels) {
  Allocator::Config cfg{};
  cfg.levels = {128, 256}; // 増加している
  orteaf::tests::ExpectError(OrteafErrc::InvalidParameter,
                             [&] { allocator_.initialize(cfg, &heap_ops_); });
}

TEST_F(HierarchicalSlotAllocatorTest, InitializeThrowsOnNonDivisibleLevels) {
  Allocator::Config cfg{};
  cfg.levels = {256, 100}; // 256 % 100 != 0
  orteaf::tests::ExpectError(OrteafErrc::InvalidParameter,
                             [&] { allocator_.initialize(cfg, &heap_ops_); });
}

// ============================================================================
// Threshold validation tests
// ============================================================================

TEST_F(HierarchicalSlotAllocatorTest, InitializeThrowsOnThresholdTooSmall) {
  Allocator::Config cfg{};
  cfg.levels = {256};
  cfg.threshold = 4; // システム最小閾値より小さい
  orteaf::tests::ExpectError(OrteafErrc::InvalidParameter,
                             [&] { allocator_.initialize(cfg, &heap_ops_); });
}

TEST_F(HierarchicalSlotAllocatorTest,
       InitializeThrowsOnNonPowerOfTwoThreshold) {
  Allocator::Config cfg{};
  cfg.levels = {256};
  cfg.threshold = 100; // 2の冪乗ではない
  orteaf::tests::ExpectError(OrteafErrc::InvalidParameter,
                             [&] { allocator_.initialize(cfg, &heap_ops_); });
}

TEST_F(HierarchicalSlotAllocatorTest,
       InitializeThrowsOnLevelBelowThresholdNotPowerOfTwo) {
  Allocator::Config cfg{};
  cfg.levels = {256, 64, 48}; // 48はthreshold=64未満で2の冪乗ではない
  cfg.threshold = 64;
  orteaf::tests::ExpectError(OrteafErrc::InvalidParameter,
                             [&] { allocator_.initialize(cfg, &heap_ops_); });
}

TEST_F(HierarchicalSlotAllocatorTest,
       InitializeThrowsOnLevelAboveThresholdNotDivisible) {
  Allocator::Config cfg{};
  cfg.levels = {300, 64}; // 300 % 64 != 0
  cfg.threshold = 64;
  orteaf::tests::ExpectError(OrteafErrc::InvalidParameter,
                             [&] { allocator_.initialize(cfg, &heap_ops_); });
}

TEST_F(HierarchicalSlotAllocatorTest, InitializeAcceptsValidThreshold) {
  void *base = reinterpret_cast<void *>(0x6000);
  EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base, 256}));

  Allocator::Config cfg{};
  cfg.levels = {256, 64, 32, 16};
  cfg.threshold = 64;
  EXPECT_NO_THROW(allocator_.initialize(cfg, &heap_ops_));
}

// ============================================================================
// expand_bytes validation tests
// ============================================================================

TEST_F(HierarchicalSlotAllocatorTest, InitializeThrowsOnInvalidExpandBytes) {
  Allocator::Config cfg{};
  cfg.levels = {256};
  cfg.expand_bytes = 100; // 256の倍数ではない
  orteaf::tests::ExpectError(OrteafErrc::InvalidParameter,
                             [&] { allocator_.initialize(cfg, &heap_ops_); });
}

TEST_F(HierarchicalSlotAllocatorTest, InitializeAcceptsValidExpandBytes) {
  void *base = reinterpret_cast<void *>(0x7000);
  EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base, 256}));

  Allocator::Config cfg{};
  cfg.levels = {256};
  cfg.expand_bytes = 512; // 256の倍数
  EXPECT_NO_THROW(allocator_.initialize(cfg, &heap_ops_));
}

TEST_F(HierarchicalSlotAllocatorTest, ExpandBytesUsedWhenAllocatingMoreSlots) {
  void *base1 = reinterpret_cast<void *>(0x8000);
  void *base2 = reinterpret_cast<void *>(0x9000);

  // 初回reserve + 拡張時のreserve
  EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base1, 256}));
  EXPECT_CALL(impl_, reserve(512)).WillOnce(Return(HeapRegion{base2, 512}));
  EXPECT_CALL(impl_, map(_)).WillRepeatedly(::testing::Invoke(MapReturn));

  Allocator::Config cfg{};
  cfg.levels = {256};
  cfg.initial_bytes = 256;
  cfg.expand_bytes = 512;
  allocator_.initialize(cfg, &heap_ops_);

  // 1つ目のスロットを使い切る
  auto view1 = allocator_.allocate(256);
  EXPECT_TRUE(view1);

  // 2つ目を要求すると拡張が発生
  auto view2 = allocator_.allocate(256);
  EXPECT_TRUE(view2);
}

// ============================================================================
// computeRequestSlots tests
// ============================================================================

TEST_F(HierarchicalSlotAllocatorTest, ComputeRequestSlotsSingleSlot) {
  // levels = {256}, size = 100 → rs = [1]
  void *base = reinterpret_cast<void *>(0xA000);
  EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base, 256}));

  Allocator::Config cfg{};
  cfg.levels = {256};
  allocator_.initialize(cfg, &heap_ops_);

  auto rs = allocator_.computeRequestSlots(100);
  ASSERT_EQ(rs.size(), 1);
  EXPECT_EQ(rs[0], 1);
}

TEST_F(HierarchicalSlotAllocatorTest, ComputeRequestSlotsExactFit) {
  // levels = {256, 64}, size = 256 → rs = [1, 0]
  void *base = reinterpret_cast<void *>(0xB000);
  EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base, 256}));

  Allocator::Config cfg{};
  cfg.levels = {256, 64};
  allocator_.initialize(cfg, &heap_ops_);

  auto rs = allocator_.computeRequestSlots(256);
  ASSERT_EQ(rs.size(), 2);
  EXPECT_EQ(rs[0], 1);
  EXPECT_EQ(rs[1], 0);
}

TEST_F(HierarchicalSlotAllocatorTest, ComputeRequestSlotsMultiLayer) {
  // levels = {256, 128, 64}, size = 300
  // b = 64, N = ceil(300/64) = 5
  // u = [4, 2, 1]
  // rs[0] = 5/4 = 1, N_rem = 1
  // rs[1] = 1/2 = 0, N_rem = 1
  // rs[2] = 1
  // → rs = [1, 0, 1]
  void *base = reinterpret_cast<void *>(0xC000);
  EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base, 256}));

  Allocator::Config cfg{};
  cfg.levels = {256, 128, 64};
  allocator_.initialize(cfg, &heap_ops_);

  auto rs = allocator_.computeRequestSlots(300);
  ASSERT_EQ(rs.size(), 3);
  EXPECT_EQ(rs[0], 1);
  EXPECT_EQ(rs[1], 0);
  EXPECT_EQ(rs[2], 1);
}

TEST_F(HierarchicalSlotAllocatorTest, ComputeRequestSlotsLargeSize) {
  // levels = {256, 64}, size = 600
  // b = 64, N = ceil(600/64) = 10
  // u = [4, 1]
  // rs[0] = 10/4 = 2, N_rem = 2
  // rs[1] = 2
  // → rs = [2, 2]
  void *base = reinterpret_cast<void *>(0xD000);
  EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base, 256}));

  Allocator::Config cfg{};
  cfg.levels = {256, 64};
  allocator_.initialize(cfg, &heap_ops_);

  auto rs = allocator_.computeRequestSlots(600);
  ASSERT_EQ(rs.size(), 2);
  EXPECT_EQ(rs[0], 2);
  EXPECT_EQ(rs[1], 2);
}

TEST_F(HierarchicalSlotAllocatorTest, ComputeRequestSlotsSmallestSlotOnly) {
  // levels = {256, 64}, size = 32
  // b = 64, N = ceil(32/64) = 1
  // u = [4, 1]
  // rs[0] = 1/4 = 0, N_rem = 1
  // rs[1] = 1
  // → rs = [0, 1]
  void *base = reinterpret_cast<void *>(0xE000);
  EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base, 256}));

  Allocator::Config cfg{};
  cfg.levels = {256, 64};
  allocator_.initialize(cfg, &heap_ops_);

  auto rs = allocator_.computeRequestSlots(32);
  ASSERT_EQ(rs.size(), 2);
  EXPECT_EQ(rs[0], 0);
  EXPECT_EQ(rs[1], 1);
}

TEST_F(HierarchicalSlotAllocatorTest, TrailPlanPicksNextFreeChildInLifoOrder) {
  // levels = {256, 64} で child を LIFO で 3,2 と消費した後、trail が child 1
  // を指すことを確認
  void *base = reinterpret_cast<void *>(0x16000);
  EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base, 256}));
  EXPECT_CALL(impl_, map(_)).WillRepeatedly(::testing::Invoke(MapReturn));

  Allocator::Config cfg{};
  cfg.levels = {256, 64};
  cfg.initial_bytes = 256;
  allocator_.initialize(cfg, &heap_ops_);

  auto first = allocator_.allocate(64);  // child 3
  auto second = allocator_.allocate(64); // child 2
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);

  // デバッグスナップショットで状態を確認
  auto snap = allocator_.debugSnapshot();
  ASSERT_EQ(snap.size(), 2u);
  const auto &layer0 = snap[0];
  const auto &layer1 = snap[1];
  // parent0 が Split になっていること
  ASSERT_FALSE(layer0.slots.empty());
  EXPECT_EQ(layer0.slots[0].state, decltype(layer0.slots[0].state)::Split);
  // child の状態: 3,2 が InUse, 1,0 が Free
  ASSERT_GE(layer1.slots.size(), 4u);
  EXPECT_EQ(layer1.slots[3].state, decltype(layer1.slots[3].state)::InUse);
  EXPECT_EQ(layer1.slots[2].state, decltype(layer1.slots[2].state)::InUse);
  EXPECT_EQ(layer1.slots[1].state, decltype(layer1.slots[1].state)::Free);
  EXPECT_EQ(layer1.slots[0].state, decltype(layer1.slots[0].state)::Free);

  for (const auto &layer : snap) {
    for (const auto &slot : layer.slots) {
      printf("Layer %zu Slot state: %d\n", &layer - &snap[0],
             static_cast<int>(slot.state));
    }
  }

  auto rs = allocator_.computeRequestSlots(64); // [0,1]
  EXPECT_EQ(rs.size(), 2u);
  EXPECT_EQ(rs[0], 0u);
  EXPECT_EQ(rs[1], 1u);
  auto plan = allocator_.debugTryFindTrailPlan(rs);
  ASSERT_TRUE(plan.found);
  EXPECT_EQ(plan.end_layer, 1u);
  printf("Trail Plan end_slot: %u\n", plan.end_slot);
  EXPECT_EQ(plan.end_slot, 1u); // 残りの LIFO 先頭は child 1
}

TEST_F(HierarchicalSlotAllocatorTest,
       TrailPlanFailsWhenChildNeededButParentNotSplit) {
  // levels = {256, 64}, size = 320 → rs = [1,1]
  // 親スロットが Split されていない状態では子を確保できないため plan
  // は見つからないはず
  void *base = reinterpret_cast<void *>(0x17000);
  EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base, 256}));

  Allocator::Config cfg{};
  cfg.levels = {256, 64};
  cfg.initial_bytes = 256;
  allocator_.initialize(cfg, &heap_ops_);

  auto rs = allocator_.computeRequestSlots(320); // [1,1]
  ASSERT_EQ(rs.size(), 2u);
  EXPECT_EQ(rs[0], 1u);
  EXPECT_EQ(rs[1], 1u);

  auto plan = allocator_.debugTryFindTrailPlan(rs);
  EXPECT_FALSE(
      plan.found); // 子が必要なのに親がSplitでないため失敗するのが期待挙動
}

TEST_F(HierarchicalSlotAllocatorTest,
       TrailPlanFindsContiguousRunAcrossMultipleSlots) {
  // levels = {256} で 512 バイト要求 → rs = [2]
  void *base = reinterpret_cast<void *>(0x1D000);
  EXPECT_CALL(impl_, reserve(512)).WillOnce(Return(HeapRegion{base, 512}));
  EXPECT_CALL(impl_, map(_)).WillRepeatedly(::testing::Invoke(MapReturn));

  Allocator::Config cfg{};
  cfg.levels = {256};
  cfg.initial_bytes = 512;
  allocator_.initialize(cfg, &heap_ops_);

  auto rs = allocator_.computeRequestSlots(400); // [2]
  ASSERT_EQ(rs.size(), 1u);
  EXPECT_EQ(rs[0], 2u);

  auto plan = allocator_.debugTryFindTrailPlan(rs);
  ASSERT_TRUE(plan.found);
  EXPECT_EQ(plan.end_layer, 0u);
  EXPECT_EQ(plan.end_slot, 1u); // 2スロット連続で使用できる末尾側
}

TEST_F(HierarchicalSlotAllocatorTest,
       TrailPlanFailsWhenContiguousRunIsShorterThanNeeded) {
  // 512 バイト確保済みスロットのうち片方を使用すると、512
  // バイト要求（rs=[2]）は失敗する
  void *base = reinterpret_cast<void *>(0x1E000);
  EXPECT_CALL(impl_, reserve(512)).WillOnce(Return(HeapRegion{base, 512}));
  EXPECT_CALL(impl_, map(_)).WillRepeatedly(::testing::Invoke(MapReturn));

  Allocator::Config cfg{};
  cfg.levels = {256};
  cfg.initial_bytes = 512;
  allocator_.initialize(cfg, &heap_ops_);

  auto first = allocator_.allocate(256);
  ASSERT_TRUE(first);

  auto rs = allocator_.computeRequestSlots(400); // [2]
  ASSERT_EQ(rs.size(), 1u);
  EXPECT_EQ(rs[0], 2u);

  auto plan = allocator_.debugTryFindTrailPlan(rs);
  EXPECT_FALSE(plan.found);
}

TEST_F(HierarchicalSlotAllocatorTest, TrailPlanHandlesMultiLevelWithChildRun) {
  // levels = {256, 64}, root の末尾スロットを Split した状態で子 2
  // 連続を要求する
  void *base = reinterpret_cast<void *>(0x1F000);
  EXPECT_CALL(impl_, reserve(512)).WillOnce(Return(HeapRegion{base, 512}));
  EXPECT_CALL(impl_, map(_)).WillRepeatedly(::testing::Invoke(MapReturn));

  Allocator::Config cfg{};
  cfg.levels = {256, 64};
  cfg.initial_bytes = 512; // root に 2 スロット
  allocator_.initialize(cfg, &heap_ops_);

  // 末尾側の root スロットを Split させるために 64 バイトを 1 度確保
  auto first = allocator_.allocate(64);
  ASSERT_TRUE(first);

  // child 2 スロットを要求する想定の rs
  std::vector<uint32_t> rs = {1, 2};
  auto plan = allocator_.debugTryFindTrailPlan(rs);
  ASSERT_TRUE(plan.found);
  EXPECT_EQ(plan.end_layer, 1u);
  EXPECT_EQ(plan.end_slot, 2u); // split した root スロット配下の末尾 Free run
}

TEST_F(HierarchicalSlotAllocatorTest, TrailPlanFailsWhenChildRunTooShort) {
  // 同じ構成で子 3 スロット連続が必要な場合は失敗する
  void *base = reinterpret_cast<void *>(0x20000);
  EXPECT_CALL(impl_, reserve(512)).WillOnce(Return(HeapRegion{base, 512}));
  EXPECT_CALL(impl_, map(_)).WillRepeatedly(::testing::Invoke(MapReturn));

  Allocator::Config cfg{};
  cfg.levels = {256, 64};
  cfg.initial_bytes = 512;
  allocator_.initialize(cfg, &heap_ops_);

  // 末尾 root を Split。child のうち 2 つが使用済みになる状態。
  auto first = allocator_.allocate(64);
  auto second = allocator_.allocate(64);
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);

  std::vector<uint32_t> rs = {1, 3};
  auto plan = allocator_.debugTryFindTrailPlan(rs);
  EXPECT_FALSE(plan.found);
}

TEST_F(HierarchicalSlotAllocatorTest, TrailPlanDepth3PicksRootWhenNoSplits) {
  // levels = {256, 64, 16}, size = 16 → rs = [0,0,1]
  // どのレイヤも Split されていないので末尾の親スロットを開始点にする
  void *base = reinterpret_cast<void *>(0x18000);
  EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base, 256}));

  Allocator::Config cfg{};
  cfg.levels = {256, 64, 16};
  cfg.initial_bytes = 256;
  allocator_.initialize(cfg, &heap_ops_);

  auto rs = allocator_.computeRequestSlots(16); // [0,0,1]
  ASSERT_EQ(rs.size(), 3u);
  EXPECT_EQ(rs[0], 0u);
  EXPECT_EQ(rs[1], 0u);
  EXPECT_EQ(rs[2], 1u);

  auto plan = allocator_.debugTryFindTrailPlan(rs);
  ASSERT_TRUE(plan.found);
  EXPECT_EQ(plan.end_layer, 0u);
  EXPECT_EQ(plan.end_slot, 0u);
}

TEST_F(HierarchicalSlotAllocatorTest, TrailPlanDepth3PicksMiddleWhenNotSplit) {
  // levels = {256, 64, 16}, size = 16 → rs = [0,0,1]
  // root を Split しただけで middle が Free のままなら middle を開始点にする
  void *base = reinterpret_cast<void *>(0x19000);
  EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base, 256}));
  EXPECT_CALL(impl_, map(_)).WillRepeatedly(::testing::Invoke(MapReturn));

  Allocator::Config cfg{};
  cfg.levels = {256, 64, 16};
  cfg.initial_bytes = 256;
  allocator_.initialize(cfg, &heap_ops_);

  // root を Split するだけの確保（64 バイト）
  auto view64 = allocator_.allocate(64);
  ASSERT_TRUE(view64);

  auto rs = allocator_.computeRequestSlots(16); // [0,0,1]
  auto plan = allocator_.debugTryFindTrailPlan(rs);
  ASSERT_TRUE(plan.found);
  EXPECT_EQ(plan.end_layer, 1u);
  EXPECT_EQ(plan.end_slot, 2u);
}

TEST_F(HierarchicalSlotAllocatorTest,
       TrailPlanDepth3PicksGrandchildInLifoOrder) {
  // levels = {256, 64, 16} で孫レイヤを LIFO で 2 つ消費した後、次の trail
  // が孫の末尾側を指すことを確認
  void *base = reinterpret_cast<void *>(0x1A000);
  EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base, 256}));
  EXPECT_CALL(impl_, map(_)).WillRepeatedly(::testing::Invoke(MapReturn));

  Allocator::Config cfg{};
  cfg.levels = {256, 64, 16};
  cfg.initial_bytes = 256;
  allocator_.initialize(cfg, &heap_ops_);

  auto first = allocator_.allocate(16);
  auto second = allocator_.allocate(16);
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);

  auto rs = allocator_.computeRequestSlots(16); // [0,0,1]
  auto plan = allocator_.debugTryFindTrailPlan(rs);
  ASSERT_TRUE(plan.found);
  EXPECT_EQ(plan.end_layer, 2u);
  EXPECT_EQ(plan.end_slot, 1u); // 末尾側から 2 つ消費済みなので、次は slot 1
}

TEST_F(HierarchicalSlotAllocatorTest,
       TrailPlanDepth3ContinuesInSameSplitBranch) {
  // 孫を1つ確保したあと、同じ Split 済みブランチで次の末尾 Free を指す
  void *base = reinterpret_cast<void *>(0x1B000);
  EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base, 256}));
  EXPECT_CALL(impl_, map(_)).WillRepeatedly(::testing::Invoke(MapReturn));

  Allocator::Config cfg{};
  cfg.levels = {256, 64, 16};
  cfg.initial_bytes = 256;
  allocator_.initialize(cfg, &heap_ops_);

  auto first = allocator_.allocate(16); // grandchild slot3 を使用
  ASSERT_TRUE(first);

  auto rs = allocator_.computeRequestSlots(16); // [0,0,1]
  auto plan = allocator_.debugTryFindTrailPlan(rs);
  ASSERT_TRUE(plan.found);
  EXPECT_EQ(plan.end_layer, 2u);
  EXPECT_EQ(plan.end_slot, 2u); // 同じブランチの末尾側 Free を選ぶ
}

TEST_F(HierarchicalSlotAllocatorTest,
       TrailPlanDepth3PicksTailmostFreeAfterHole) {
  // 孫を2つ確保した後、片方を解放して穴を作り、末尾側の連続Freeを選ぶことを確認
  void *base = reinterpret_cast<void *>(0x1C000);
  EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base, 256}));
  EXPECT_CALL(impl_, map(_)).WillRepeatedly(::testing::Invoke(MapReturn));
  EXPECT_CALL(impl_, unmap(_, 16)).Times(1);

  Allocator::Config cfg{};
  cfg.levels = {256, 64, 16};
  cfg.initial_bytes = 256;
  allocator_.initialize(cfg, &heap_ops_);

  auto first = allocator_.allocate(16);  // grandchild slot3
  auto second = allocator_.allocate(16); // grandchild slot2
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);

  allocator_.deallocate(second); // slot2 を解放して穴を作る

  auto rs = allocator_.computeRequestSlots(16); // [0,0,1]
  auto plan = allocator_.debugTryFindTrailPlan(rs);
  ASSERT_TRUE(plan.found);
  EXPECT_EQ(plan.end_layer, 2u);
  EXPECT_EQ(plan.end_slot, 2u); // 末尾側で連続Freeの run(=1) を選ぶ
}

// //
// ============================================================================
// // Dense allocation tests
// //
// ============================================================================

// TEST_F(HierarchicalSlotAllocatorTest, AllocateDenseMultipleSlots) {
//     // levels = {256, 128, 64}, size = 300 → rs = [1, 0, 1]
//     // 連続した領域として確保される
//     void* base = reinterpret_cast<void*>(0xF000);
//     EXPECT_CALL(impl_, reserve(512)).WillOnce(Return(HeapRegion{base, 512}));
//     EXPECT_CALL(impl_, map(_)).WillRepeatedly(::testing::Invoke(MapReturn));

//     Allocator::Config cfg{};
//     cfg.levels = {256, 128, 64};
//     cfg.initial_bytes = 512;
//     allocator_.initialize(cfg, &heap_ops_);

//     auto view = allocator_.allocateDense(300);
//     EXPECT_TRUE(view);
//     // 256 + 64 = 320 bytes
//     EXPECT_GE(view.size(), 300);
// }

// TEST_F(HierarchicalSlotAllocatorTest, DeallocateDenseReleasesAllSlots) {
//     void* base = reinterpret_cast<void*>(0x10000);
//     EXPECT_CALL(impl_, reserve(512)).WillOnce(Return(HeapRegion{base, 512}));
//     EXPECT_CALL(impl_, map(_)).WillRepeatedly(::testing::Invoke(MapReturn));
//     // unmap for 256 + 64 = 2 calls
//     EXPECT_CALL(impl_, unmap(_, _)).Times(::testing::AtLeast(1));

//     Allocator::Config cfg{};
//     cfg.levels = {256, 128, 64};
//     cfg.initial_bytes = 512;
//     allocator_.initialize(cfg, &heap_ops_);

//     const std::size_t request_size = 300;
//     auto view = allocator_.allocateDense(request_size);
//     allocator_.deallocateDense(view, request_size);
// }

// TEST_F(HierarchicalSlotAllocatorTest, AllocateDenseSingleSlotLevel0) {
//     // levels = {256, 128, 64}, size = 200 → rs = [1, 0, 0]
//     void* base = reinterpret_cast<void*>(0x11000);
//     EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base, 256}));
//     EXPECT_CALL(impl_, map(_)).WillOnce(::testing::Invoke(MapReturn));

//     Allocator::Config cfg{};
//     cfg.levels = {256, 128, 64};
//     cfg.initial_bytes = 256;
//     allocator_.initialize(cfg, &heap_ops_);

//     auto view = allocator_.allocateDense(200);
//     EXPECT_TRUE(view);
//     EXPECT_EQ(view.size(), 256);
// }

// TEST_F(HierarchicalSlotAllocatorTest, AllocateDenseSingleSlotLevel1) {
//     // levels = {256, 128, 64}, size = 100 → rs = [0, 1, 0]
//     void* base = reinterpret_cast<void*>(0x12000);
//     EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base, 256}));
//     EXPECT_CALL(impl_, map(_)).WillOnce(::testing::Invoke(MapReturn));

//     Allocator::Config cfg{};
//     cfg.levels = {256, 128, 64};
//     cfg.initial_bytes = 256;
//     allocator_.initialize(cfg, &heap_ops_);

//     auto view = allocator_.allocateDense(100);
//     EXPECT_TRUE(view);
//     EXPECT_EQ(view.size(), 128);
// }

// TEST_F(HierarchicalSlotAllocatorTest, AllocateDenseSingleSlotLevel2) {
//     // levels = {256, 128, 64}, size = 50 → rs = [0, 0, 1]
//     void* base = reinterpret_cast<void*>(0x13000);
//     EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base, 256}));
//     EXPECT_CALL(impl_, map(_)).WillOnce(::testing::Invoke(MapReturn));

//     Allocator::Config cfg{};
//     cfg.levels = {256, 128, 64};
//     cfg.initial_bytes = 256;
//     allocator_.initialize(cfg, &heap_ops_);

//     auto view = allocator_.allocateDense(50);
//     EXPECT_TRUE(view);
//     EXPECT_EQ(view.size(), 64);
// }

// TEST_F(HierarchicalSlotAllocatorTest,
// AllocateDenseFromChildStartContinuesContiguously) {
//     //
//     最初の割り当てでスプリットが走り、次の割り当ては子レイヤから開始するケース。
//     // 2回目は子スロットが連続して割り当てられることを確認する。
//     void* base = reinterpret_cast<void*>(0x14000);
//     EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base, 256}));
//     EXPECT_CALL(impl_, map(_)).WillRepeatedly(::testing::Invoke(MapReturn));

//     Allocator::Config cfg{};
//     cfg.levels = {256, 128, 64};
//     cfg.initial_bytes = 256;
//     allocator_.initialize(cfg, &heap_ops_);

//     auto first = allocator_.allocateDense(64);   // 子レイヤの最初のスロット
//     auto second = allocator_.allocateDense(64);  //
//     子レイヤの次のスロットに連続で入るはず

//     ASSERT_TRUE(first);
//     ASSERT_TRUE(second);
//     auto diff = static_cast<std::uintptr_t>(static_cast<char*>(second.data())
//     - static_cast<char*>(first.data())); EXPECT_EQ(diff, 64);  //
//     子スロットが連続していることを確認
// }

} // namespace

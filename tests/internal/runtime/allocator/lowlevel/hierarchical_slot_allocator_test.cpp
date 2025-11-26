#include "orteaf/internal/runtime/allocator/lowlevel/hierarchical_slot_allocator.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/backend/backend_traits.h"
#include "tests/internal/runtime/allocator/testing/mock_resource.h"
#include "tests/internal/testing/error_assert.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace allocator = ::orteaf::internal::runtime::allocator;
namespace policies = ::orteaf::internal::runtime::allocator::policies;
using Backend = ::orteaf::internal::backend::Backend;
using Traits = ::orteaf::internal::backend::BackendTraits<Backend::Cpu>;
using BufferView = Traits::BufferView;
using HeapRegion = Traits::HeapRegion;
using ::orteaf::internal::runtime::allocator::testing::MockCpuHeapOps;
using ::orteaf::internal::runtime::allocator::testing::MockCpuHeapOpsImpl;
using OrteafErrc = ::orteaf::internal::diagnostics::error::OrteafErrc;

namespace {

using Allocator = policies::HierarchicalSlotAllocator<MockCpuHeapOps, Backend::Cpu>;

static BufferView MapReturn(HeapRegion region) {
    return BufferView{region.data(), 0, region.size()};
}

class HierarchicalSlotAllocatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        MockCpuHeapOps::set(&impl_);
    }

    void TearDown() override {
        MockCpuHeapOps::reset();
    }

    NiceMock<MockCpuHeapOpsImpl> impl_;
    MockCpuHeapOps heap_ops_;
    Allocator allocator_;
};

TEST_F(HierarchicalSlotAllocatorTest, AllocateReturnsBufferView) {
    void* base = reinterpret_cast<void*>(0x1000);
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
    void* base = reinterpret_cast<void*>(0x2000);
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
    void* base = reinterpret_cast<void*>(0x3000);
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
    void* base = reinterpret_cast<void*>(0x4000);
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
    orteaf::tests::ExpectError(OrteafErrc::InvalidParameter, [&] {
        allocator_.initialize(cfg, &heap_ops_);
    });
}

TEST_F(HierarchicalSlotAllocatorTest, InitializeThrowsOnInvalidInitialBytes) {
    Allocator::Config cfg{};
    cfg.levels = {256};
    cfg.initial_bytes = 100;  // 256の倍数ではない
    orteaf::tests::ExpectError(OrteafErrc::InvalidParameter, [&] {
        allocator_.initialize(cfg, &heap_ops_);
    });
}

TEST_F(HierarchicalSlotAllocatorTest, InitializeAcceptsValidInitialBytes) {
    void* base = reinterpret_cast<void*>(0x5000);
    EXPECT_CALL(impl_, reserve(512)).WillOnce(Return(HeapRegion{base, 512}));

    Allocator::Config cfg{};
    cfg.levels = {256};
    cfg.initial_bytes = 512;  // 256の倍数
    EXPECT_NO_THROW(allocator_.initialize(cfg, &heap_ops_));
}

TEST_F(HierarchicalSlotAllocatorTest, InitializeThrowsOnZeroLevel) {
    Allocator::Config cfg{};
    cfg.levels = {256, 0, 64};
    orteaf::tests::ExpectError(OrteafErrc::InvalidParameter, [&] {
        allocator_.initialize(cfg, &heap_ops_);
    });
}

TEST_F(HierarchicalSlotAllocatorTest, InitializeThrowsOnNonDecreasingLevels) {
    Allocator::Config cfg{};
    cfg.levels = {128, 256};  // 増加している
    orteaf::tests::ExpectError(OrteafErrc::InvalidParameter, [&] {
        allocator_.initialize(cfg, &heap_ops_);
    });
}

TEST_F(HierarchicalSlotAllocatorTest, InitializeThrowsOnNonDivisibleLevels) {
    Allocator::Config cfg{};
    cfg.levels = {256, 100};  // 256 % 100 != 0
    orteaf::tests::ExpectError(OrteafErrc::InvalidParameter, [&] {
        allocator_.initialize(cfg, &heap_ops_);
    });
}

// ============================================================================
// Threshold validation tests
// ============================================================================

TEST_F(HierarchicalSlotAllocatorTest, InitializeThrowsOnThresholdTooSmall) {
    Allocator::Config cfg{};
    cfg.levels = {256};
    cfg.threshold = 4;  // システム最小閾値より小さい
    orteaf::tests::ExpectError(OrteafErrc::InvalidParameter, [&] {
        allocator_.initialize(cfg, &heap_ops_);
    });
}

TEST_F(HierarchicalSlotAllocatorTest, InitializeThrowsOnNonPowerOfTwoThreshold) {
    Allocator::Config cfg{};
    cfg.levels = {256};
    cfg.threshold = 100;  // 2の冪乗ではない
    orteaf::tests::ExpectError(OrteafErrc::InvalidParameter, [&] {
        allocator_.initialize(cfg, &heap_ops_);
    });
}

TEST_F(HierarchicalSlotAllocatorTest, InitializeThrowsOnLevelBelowThresholdNotPowerOfTwo) {
    Allocator::Config cfg{};
    cfg.levels = {256, 64, 48};  // 48はthreshold=64未満で2の冪乗ではない
    cfg.threshold = 64;
    orteaf::tests::ExpectError(OrteafErrc::InvalidParameter, [&] {
        allocator_.initialize(cfg, &heap_ops_);
    });
}

TEST_F(HierarchicalSlotAllocatorTest, InitializeThrowsOnLevelAboveThresholdNotDivisible) {
    Allocator::Config cfg{};
    cfg.levels = {300, 64};  // 300 % 64 != 0
    cfg.threshold = 64;
    orteaf::tests::ExpectError(OrteafErrc::InvalidParameter, [&] {
        allocator_.initialize(cfg, &heap_ops_);
    });
}

TEST_F(HierarchicalSlotAllocatorTest, InitializeAcceptsValidThreshold) {
    void* base = reinterpret_cast<void*>(0x6000);
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
    cfg.expand_bytes = 100;  // 256の倍数ではない
    orteaf::tests::ExpectError(OrteafErrc::InvalidParameter, [&] {
        allocator_.initialize(cfg, &heap_ops_);
    });
}

TEST_F(HierarchicalSlotAllocatorTest, InitializeAcceptsValidExpandBytes) {
    void* base = reinterpret_cast<void*>(0x7000);
    EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base, 256}));

    Allocator::Config cfg{};
    cfg.levels = {256};
    cfg.expand_bytes = 512;  // 256の倍数
    EXPECT_NO_THROW(allocator_.initialize(cfg, &heap_ops_));
}

TEST_F(HierarchicalSlotAllocatorTest, ExpandBytesUsedWhenAllocatingMoreSlots) {
    void* base1 = reinterpret_cast<void*>(0x8000);
    void* base2 = reinterpret_cast<void*>(0x9000);
    
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
    void* base = reinterpret_cast<void*>(0xA000);
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
    void* base = reinterpret_cast<void*>(0xB000);
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
    void* base = reinterpret_cast<void*>(0xC000);
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
    void* base = reinterpret_cast<void*>(0xD000);
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
    void* base = reinterpret_cast<void*>(0xE000);
    EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base, 256}));

    Allocator::Config cfg{};
    cfg.levels = {256, 64};
    allocator_.initialize(cfg, &heap_ops_);

    auto rs = allocator_.computeRequestSlots(32);
    ASSERT_EQ(rs.size(), 2);
    EXPECT_EQ(rs[0], 0);
    EXPECT_EQ(rs[1], 1);
}

// ============================================================================
// Dense allocation tests
// ============================================================================

TEST_F(HierarchicalSlotAllocatorTest, AllocateDenseMultipleSlots) {
    // levels = {256, 128, 64}, size = 300 → rs = [1, 0, 1]
    // 連続した領域として確保される
    void* base = reinterpret_cast<void*>(0xF000);
    EXPECT_CALL(impl_, reserve(512)).WillOnce(Return(HeapRegion{base, 512}));
    EXPECT_CALL(impl_, map(_)).WillRepeatedly(::testing::Invoke(MapReturn));

    Allocator::Config cfg{};
    cfg.levels = {256, 128, 64};
    cfg.initial_bytes = 512;
    allocator_.initialize(cfg, &heap_ops_);

    auto view = allocator_.allocateDense(300);
    EXPECT_TRUE(view);
    // 256 + 64 = 320 bytes
    EXPECT_GE(view.size(), 300);
}

TEST_F(HierarchicalSlotAllocatorTest, DeallocateDenseReleasesAllSlots) {
    void* base = reinterpret_cast<void*>(0x10000);
    EXPECT_CALL(impl_, reserve(512)).WillOnce(Return(HeapRegion{base, 512}));
    EXPECT_CALL(impl_, map(_)).WillRepeatedly(::testing::Invoke(MapReturn));
    // unmap for 256 + 64 = 2 calls
    EXPECT_CALL(impl_, unmap(_, _)).Times(::testing::AtLeast(1));

    Allocator::Config cfg{};
    cfg.levels = {256, 128, 64};
    cfg.initial_bytes = 512;
    allocator_.initialize(cfg, &heap_ops_);

    auto view = allocator_.allocateDense(300);
    allocator_.deallocateDense(view);
}

TEST_F(HierarchicalSlotAllocatorTest, AllocateDenseSingleSlotLevel0) {
    // levels = {256, 128, 64}, size = 200 → rs = [1, 0, 0]
    void* base = reinterpret_cast<void*>(0x11000);
    EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base, 256}));
    EXPECT_CALL(impl_, map(_)).WillOnce(::testing::Invoke(MapReturn));

    Allocator::Config cfg{};
    cfg.levels = {256, 128, 64};
    cfg.initial_bytes = 256;
    allocator_.initialize(cfg, &heap_ops_);

    auto view = allocator_.allocateDense(200);
    EXPECT_TRUE(view);
    EXPECT_EQ(view.size(), 256);
}

TEST_F(HierarchicalSlotAllocatorTest, AllocateDenseSingleSlotLevel1) {
    // levels = {256, 128, 64}, size = 100 → rs = [0, 1, 0]
    void* base = reinterpret_cast<void*>(0x12000);
    EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base, 256}));
    EXPECT_CALL(impl_, map(_)).WillOnce(::testing::Invoke(MapReturn));

    Allocator::Config cfg{};
    cfg.levels = {256, 128, 64};
    cfg.initial_bytes = 256;
    allocator_.initialize(cfg, &heap_ops_);

    auto view = allocator_.allocateDense(100);
    EXPECT_TRUE(view);
    EXPECT_EQ(view.size(), 128);
}

TEST_F(HierarchicalSlotAllocatorTest, AllocateDenseSingleSlotLevel2) {
    // levels = {256, 128, 64}, size = 50 → rs = [0, 0, 1]
    void* base = reinterpret_cast<void*>(0x13000);
    EXPECT_CALL(impl_, reserve(256)).WillOnce(Return(HeapRegion{base, 256}));
    EXPECT_CALL(impl_, map(_)).WillOnce(::testing::Invoke(MapReturn));

    Allocator::Config cfg{};
    cfg.levels = {256, 128, 64};
    cfg.initial_bytes = 256;
    allocator_.initialize(cfg, &heap_ops_);

    auto view = allocator_.allocateDense(50);
    EXPECT_TRUE(view);
    EXPECT_EQ(view.size(), 64);
}

}  // namespace

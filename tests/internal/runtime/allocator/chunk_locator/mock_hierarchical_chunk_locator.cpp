#include "orteaf/internal/runtime/allocator/policies/chunk_locator/hierarchical_chunk_locator.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <system_error>
#include <sstream>
#include <thread>
#include <vector>

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/backend/backend_traits.h"
#include "tests/internal/runtime/allocator/testing/mock_resource.h"
#include "tests/internal/testing/error_assert.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnArg;

namespace allocator = ::orteaf::internal::runtime::allocator;
namespace policies = ::orteaf::internal::runtime::allocator::policies;
using Backend = ::orteaf::internal::backend::Backend;
using Traits = ::orteaf::internal::backend::BackendTraits<Backend::Cpu>;
using Device = Traits::Device;
using Context = Traits::Context;
using Stream = Traits::Stream;
using BufferView = Traits::BufferView;
using BufferId = ::orteaf::internal::base::BufferId;
using ::orteaf::internal::runtime::allocator::testing::MockCpuResource;
using ::orteaf::internal::runtime::allocator::testing::MockCpuResourceImpl;

namespace {

using Policy = policies::HierarchicalChunkLocator<MockCpuResource, Backend::Cpu>;

template <typename Entry>
void AppendSpanEntry(std::ostringstream& os, const Entry& e) {
    os << " (" << e << ",1)";
}

template <>
void AppendSpanEntry<std::pair<uint32_t, uint32_t>>(std::ostringstream& os, const std::pair<uint32_t, uint32_t>& e) {
    os << " (" << e.first << "," << e.second << ")";
}

std::string DumpSnapshot(const Policy& policy, const char* title) {
    std::ostringstream os;
    os << title << "\n";
    auto snap = policy.snapshot();
    for (std::size_t li = 0; li < snap.layers.size(); ++li) {
        const auto& L = snap.layers[li];
        os << "layer " << li << " chunk=" << L.chunk_size << "\n";
        os << "  free_list:";
        for (auto v : L.free_list) os << " " << v;
        os << "\n  span_free:";
        for (auto e : L.span_free) {
            AppendSpanEntry(os, e);
        }
        os << "\n  slots:\n";
        for (std::size_t si = 0; si < L.slots.size(); ++si) {
            const auto& s = L.slots[si];
            os << "    [" << si << "] state=" << static_cast<int>(s.state)
               << " mapped=" << s.mapped
               << " parent=" << s.parent_slot
               << " child_layer=" << s.child_layer
               << " begin=" << s.child_begin
               << " count=" << s.child_count << "\n";
        }
    }
    return os.str();
}

// テスト用フィクスチャ
class HierarchicalChunkLocatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        MockCpuResource::set(&impl_);
    }

    void TearDown() override {
        MockCpuResource::reset();
    }

    NiceMock<MockCpuResourceImpl> impl_;
    MockCpuResource resource_;
    Policy policy_;
    Device device_{1};
    Context context_{0};
    Stream stream_{nullptr};
};

TEST(HierarchicalChunkLocator, InitializeReservesRootAndReusesBlocks) {
    Policy policy;
    MockCpuResource resource;
    Device device = 5;
    Context context = 0;
    Stream stream = nullptr;
    typename Policy::Config cfg{device, context, stream, {256, 128}, /*initial_bytes=*/256, /*region_multiplier=*/1};

    NiceMock<MockCpuResourceImpl> impl;
    MockCpuResource::set(&impl);

    void* base = reinterpret_cast<void*>(0x1800);
    EXPECT_CALL(impl, reserve(256, device, stream))
        .Times(1)
        .WillOnce(Return(BufferView{base, 0, 256}));
    EXPECT_CALL(impl, map(_, device, context, stream)).Times(2).WillRepeatedly(ReturnArg<0>());
    EXPECT_CALL(impl, unmap(_, _, device, context, stream)).Times(2);

    policy.initialize(cfg, &resource);

    auto b1 = policy.addChunk(128, 1);
    EXPECT_TRUE(policy.releaseChunk(b1.id));
    auto b2 = policy.addChunk(128, 1);
    EXPECT_TRUE(policy.releaseChunk(b2.id));

    MockCpuResource::reset();
}

TEST(HierarchicalChunkLocator, InitializeFailsWithNullResource) {
    Policy policy;
    MockCpuResource resource;
    typename Policy::Config cfg{/*device=*/1, /*context=*/0, /*stream=*/nullptr, {128}, /*initial_bytes=*/128, /*region_multiplier=*/1};
    orteaf::tests::ExpectError(::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
                               [&] { policy.initialize(cfg, nullptr); });
}

TEST(HierarchicalChunkLocator, InitializeCanBeCalledTwice) {
    Policy policy;
    MockCpuResource resource;
    Device device = 2;
    Context context = 0;
    Stream stream = nullptr;
    typename Policy::Config cfg{device, context, stream, {256}, /*initial_bytes=*/256, /*region_multiplier=*/1};

    NiceMock<MockCpuResourceImpl> impl;
    MockCpuResource::set(&impl);

    void* base1 = reinterpret_cast<void*>(0x1200);
    void* base2 = reinterpret_cast<void*>(0x2200);
    EXPECT_CALL(impl, reserve(256, device, stream))
        .Times(2)
        .WillOnce(Return(BufferView{base1, 0, 256}))
        .WillOnce(Return(BufferView{base2, 0, 256}));

    policy.initialize(cfg, &resource);
    auto b1 = policy.addChunk(256, 1);
    EXPECT_TRUE(policy.releaseChunk(b1.id));

    // 2回目の initialize でも reserve が呼ばれ、再初期化できることを確認
    EXPECT_NO_THROW(policy.initialize(cfg, &resource));
    auto b2 = policy.addChunk(256, 1);
    EXPECT_TRUE(policy.releaseChunk(b2.id));

    MockCpuResource::reset();
}

TEST(HierarchicalChunkLocator, InitializeFailsWhenReserveThrows) {
    Policy policy;
    MockCpuResource resource;
    Device device = 3;
    Context context = 0;
    Stream stream = nullptr;
    typename Policy::Config cfg{device, context, stream, {256}, /*initial_bytes=*/256, /*region_multiplier=*/1};

    NiceMock<MockCpuResourceImpl> impl;
    MockCpuResource::set(&impl);

    EXPECT_CALL(impl, reserve(256, device, stream))
        .WillOnce([](std::size_t, Device, Stream) -> BufferView {
            throw std::system_error(std::make_error_code(std::errc::invalid_argument));
        });

    orteaf::tests::ExpectException<std::system_error>([&] { policy.initialize(cfg, &resource); });
    MockCpuResource::reset();
}

TEST(HierarchicalChunkLocator, AllocateWithoutInitializeThrows) {
    Policy policy;
    orteaf::tests::ExpectError(::orteaf::internal::diagnostics::error::OrteafErrc::OutOfMemory,
                               [&] { policy.addChunk(64, 1); });
}

TEST(HierarchicalChunkLocator, AddChunkFailsWhenMapThrows) {
    Policy policy;
    MockCpuResource resource;
    Device device = 4;
    Context context = 0;
    Stream stream = nullptr;
    typename Policy::Config cfg{device, context, stream, {256}, /*initial_bytes=*/256, /*region_multiplier=*/1};

    NiceMock<MockCpuResourceImpl> impl;
    MockCpuResource::set(&impl);

    void* base = reinterpret_cast<void*>(0x1400);
    EXPECT_CALL(impl, reserve(256, device, stream))
        .WillOnce(Return(BufferView{base, 0, 256}));
    EXPECT_CALL(impl, map(_, device, context, stream))
        .WillOnce([](BufferView, Device, Context, Stream) -> BufferView {
            throw std::system_error(std::make_error_code(std::errc::bad_message));
        });

    policy.initialize(cfg, &resource);
    orteaf::tests::ExpectException<std::system_error>([&] { policy.addChunk(128, 1); });

    MockCpuResource::reset();
}

TEST(HierarchicalChunkLocator, LevelsMustBeNonIncreasing) {
    Policy policy;
    MockCpuResource resource;
    typename Policy::Config cfg{/*device=*/1, /*context=*/0, /*stream=*/nullptr, {128, 256}, /*initial_bytes=*/256, /*region_multiplier=*/1};
    orteaf::tests::ExpectError(::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
                               [&] { policy.initialize(cfg, &resource); });
}

TEST(HierarchicalChunkLocator, ThresholdValidationFailsForInvalidValues) {
    Policy policy;
    MockCpuResource resource;

    // threshold がシステム最小より小さい
    typename Policy::Config cfg_small{/*device=*/1, /*context=*/0, /*stream=*/nullptr, {128}, /*initial_bytes=*/128, /*region_multiplier=*/1, /*threshold=*/alignof(double) / 2};
    orteaf::tests::ExpectError(::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
                               [&] { policy.initialize(cfg_small, &resource); });

    // threshold が非2の冪乗
    typename Policy::Config cfg_non_pow2{/*device=*/1, /*context=*/0, /*stream=*/nullptr, {128}, /*initial_bytes=*/128, /*region_multiplier=*/1, /*threshold=*/24};
    orteaf::tests::ExpectError(::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
                               [&] { policy.initialize(cfg_non_pow2, &resource); });

    // threshold 以下のレベルが 2 の冪乗でない
    typename Policy::Config cfg_below{/*device=*/1, /*context=*/0, /*stream=*/nullptr, {96, 48, 24}, /*initial_bytes=*/96, /*region_multiplier=*/1, /*threshold=*/32};
    orteaf::tests::ExpectError(::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
                               [&] { policy.initialize(cfg_below, &resource); });

    // threshold より大きいレベルが threshold で割り切れない
    typename Policy::Config cfg_above{/*device=*/1, /*context=*/0, /*stream=*/nullptr, {192, 96, 48}, /*initial_bytes=*/192, /*region_multiplier=*/1, /*threshold=*/64};
    orteaf::tests::ExpectError(::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
                               [&] { policy.initialize(cfg_above, &resource); });

    // 妥当な threshold 設定では初期化が成功する
    typename Policy::Config cfg_ok{/*device=*/1, /*context=*/0, /*stream=*/nullptr, {256, 128, 64}, /*initial_bytes=*/256, /*region_multiplier=*/1, /*threshold=*/64};
    EXPECT_NO_THROW(policy.initialize(cfg_ok, &resource));
}

TEST(HierarchicalChunkLocator, ReusesSpanWithoutExtraReserve) {
    Policy policy;
    MockCpuResource resource;
    Device device = 7;
    Context context = 0;
    Stream stream = nullptr;
    typename Policy::Config cfg{device, context, stream, {256, 128}, /*initial_bytes=*/562, /*region_multiplier=*/1};

    NiceMock<MockCpuResourceImpl> impl;
    MockCpuResource::set(&impl);

    void* base = reinterpret_cast<void*>(0x1000);
    EXPECT_CALL(impl, reserve(562, device, stream))
        .Times(1)
        .WillOnce([&](std::size_t, Device, Stream) {
            return BufferView{base, 0, 256};
        });

    EXPECT_CALL(impl, map(_, device, context, stream)).Times(4).WillRepeatedly(ReturnArg<0>());
    EXPECT_CALL(impl, unmap(_, _, device, context, stream)).Times(4);

    policy.initialize(cfg, &resource);
    auto b1 = policy.addChunk(128, 1);
    auto b2 = policy.addChunk(128, 1);
    EXPECT_TRUE(policy.releaseChunk(b1.id));
    EXPECT_TRUE(policy.releaseChunk(b2.id));

    auto b3 = policy.addChunk(128, 1);
    auto b4 = policy.addChunk(128, 1);
    EXPECT_TRUE(policy.releaseChunk(b3.id));
    EXPECT_TRUE(policy.releaseChunk(b4.id));

    MockCpuResource::reset();
}

TEST(HierarchicalChunkLocator, SplitMergeAndReuseAcrossLayers) {
    Policy policy;
    MockCpuResource resource;
    Device device = 1;
    Context context = 0;
    Stream stream = nullptr;
    typename Policy::Config cfg{device, context, stream, {512, 256, 128}, /*initial_bytes=*/512, /*region_multiplier=*/1};

    NiceMock<MockCpuResourceImpl> impl;
    MockCpuResource::set(&impl);

    void* base = reinterpret_cast<void*>(0x2000);
    EXPECT_CALL(impl, reserve(512, device, stream))
        .Times(1)
        .WillOnce(Return(BufferView{base, 0, 512}));

    EXPECT_CALL(impl, map(_, device, context, stream)).Times(6).WillRepeatedly(ReturnArg<0>());
    EXPECT_CALL(impl, unmap(_, _, device, context, stream)).Times(6);

    policy.initialize(cfg, &resource);
    auto b1 = policy.addChunk(128, 1);
    auto b2 = policy.addChunk(128, 1);
    auto b3 = policy.addChunk(128, 1);
    auto b4 = policy.addChunk(128, 1);

    std::cerr << DumpSnapshot(policy, "after 4 alloc") << std::endl;
    EXPECT_TRUE(policy.releaseChunk(b1.id));
    EXPECT_TRUE(policy.releaseChunk(b2.id));
    EXPECT_TRUE(policy.releaseChunk(b3.id));
    EXPECT_TRUE(policy.releaseChunk(b4.id));

    std::cerr << DumpSnapshot(policy, "after free all") << std::endl;
    auto b5 = policy.addChunk(128, 1);
    auto b6 = policy.addChunk(128, 1);
    std::cerr << DumpSnapshot(policy, "after re-alloc") << std::endl;
    EXPECT_TRUE(policy.releaseChunk(b5.id));
    EXPECT_TRUE(policy.releaseChunk(b6.id));
    std::cerr << DumpSnapshot(policy, "final state") << std::endl;
    policy.validate();

    MockCpuResource::reset();
}

#if ORTEAF_CORE_DEBUG_ENABLED
TEST(HierarchicalChunkLocator, LargeIdThrowsInDebug) {
    Policy policy;
    MockCpuResource resource;
    Device device = 3;
    Context context = 0;
    Stream stream = nullptr;
    typename Policy::Config cfg{device, context, stream, {256}, /*initial_bytes=*/256, /*region_multiplier=*/1};

    NiceMock<MockCpuResourceImpl> impl;
    MockCpuResource::set(&impl);
    EXPECT_CALL(impl, reserve(256, device, stream))
        .WillOnce(Return(BufferView{reinterpret_cast<void*>(0x3000), 0, 256}));

    policy.initialize(cfg, &resource);
    ::orteaf::internal::base::BufferId bad{static_cast<::orteaf::internal::base::BufferId::underlying_type>(1u << 31)};
    orteaf::tests::ExpectError(::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
                               [&] { policy.releaseChunk(bad); });
    MockCpuResource::reset();
}
#endif

// ============================================================================
// findChunkSize のテスト
// ============================================================================

TEST_F(HierarchicalChunkLocatorTest, FindChunkSizeReturnsCorrectSize) {
    typename Policy::Config cfg{device_, context_, stream_, {512, 256, 128}, /*initial_bytes=*/512, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0x4000);
    EXPECT_CALL(impl_, reserve(512, device_, stream_))
        .WillRepeatedly(Return(BufferView{base, 0, 512}));
    EXPECT_CALL(impl_, map(_, device_, context_, stream_)).WillRepeatedly(ReturnArg<0>());

    policy_.initialize(cfg, &resource_);

    // 128バイトのチャンクを割り当て
    auto b1 = policy_.addChunk(128, 1);
    EXPECT_EQ(policy_.findChunkSize(b1.id), 128);

    // 256バイトのチャンクを割り当て
    auto b2 = policy_.addChunk(256, 1);
    EXPECT_EQ(policy_.findChunkSize(b2.id), 256);

    // 512バイトのチャンクを割り当て
    auto b3 = policy_.addChunk(512, 1);
    EXPECT_EQ(policy_.findChunkSize(b3.id), 512);
}

TEST_F(HierarchicalChunkLocatorTest, FindChunkSizeReturnsZeroForInvalidId) {
    typename Policy::Config cfg{device_, context_, stream_, {256}, /*initial_bytes=*/256, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0x4000);
    EXPECT_CALL(impl_, reserve(256, device_, stream_))
        .WillOnce(Return(BufferView{base, 0, 256}));

    policy_.initialize(cfg, &resource_);

    // 存在しないID
    BufferId invalid{99999};
    EXPECT_EQ(policy_.findChunkSize(invalid), 0);
}

TEST_F(HierarchicalChunkLocatorTest, IsAliveReflectsSlotState) {
    typename Policy::Config cfg{device_, context_, stream_, {256}, /*initial_bytes=*/256, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0x4A00);
    EXPECT_CALL(impl_, reserve(256, device_, stream_))
        .WillOnce(Return(BufferView{base, 0, 256}));
    EXPECT_CALL(impl_, map(_, device_, context_, stream_)).Times(1).WillOnce(ReturnArg<0>());
    EXPECT_CALL(impl_, unmap(_, _, device_, context_, stream_)).Times(1);

    policy_.initialize(cfg, &resource_);
    auto block = policy_.addChunk(256, 1);

    EXPECT_TRUE(policy_.isAlive(block.id));

    EXPECT_TRUE(policy_.releaseChunk(block.id));
    EXPECT_FALSE(policy_.isAlive(block.id));

    BufferId invalid{55555};
    EXPECT_FALSE(policy_.isAlive(invalid));
}

// ============================================================================
// incrementUsed / decrementUsed のテスト
// ============================================================================

TEST_F(HierarchicalChunkLocatorTest, IncrementAndDecrementUsed) {
    typename Policy::Config cfg{device_, context_, stream_, {256}, /*initial_bytes=*/256, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0x5000);
    EXPECT_CALL(impl_, reserve(256, device_, stream_))
        .WillOnce(Return(BufferView{base, 0, 256}));
    EXPECT_CALL(impl_, map(_, device_, context_, stream_)).Times(1).WillOnce(ReturnArg<0>());
    // 最後の releaseChunk 成功時に unmap が1回呼ばれる
    EXPECT_CALL(impl_, unmap(_, _, device_, context_, stream_)).Times(1);

    policy_.initialize(cfg, &resource_);
    auto b = policy_.addChunk(256, 1);

    // 初期状態では used=0 なので releaseChunk 可能
    // incrementUsed でカウントアップ
    policy_.incrementUsed(b.id);
    policy_.incrementUsed(b.id);

    // used > 0 なので解放できない
    EXPECT_FALSE(policy_.releaseChunk(b.id));

    // decrementUsed でカウントダウン
    policy_.decrementUsed(b.id);
    EXPECT_FALSE(policy_.releaseChunk(b.id));

    policy_.decrementUsed(b.id);
    // used=0 になったので解放可能
    EXPECT_TRUE(policy_.releaseChunk(b.id));
}

TEST_F(HierarchicalChunkLocatorTest, DecrementUsedDoesNotUnderflow) {
    typename Policy::Config cfg{device_, context_, stream_, {256}, /*initial_bytes=*/256, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0x5100);
    EXPECT_CALL(impl_, reserve(256, device_, stream_))
        .WillOnce(Return(BufferView{base, 0, 256}));
    EXPECT_CALL(impl_, map(_, device_, context_, stream_)).WillRepeatedly(ReturnArg<0>());
    EXPECT_CALL(impl_, unmap(_, _, device_, context_, stream_)).Times(1);

    policy_.initialize(cfg, &resource_);
    auto b = policy_.addChunk(256, 1);

    // 0の状態からdecrementしてもアンダーフローしない
    policy_.decrementUsed(b.id);
    policy_.decrementUsed(b.id);

    // それでも解放可能
    EXPECT_TRUE(policy_.releaseChunk(b.id));
}

// ============================================================================
// incrementPending / decrementPending のテスト
// ============================================================================

TEST_F(HierarchicalChunkLocatorTest, IncrementAndDecrementPending) {
    typename Policy::Config cfg{device_, context_, stream_, {256}, /*initial_bytes=*/256, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0x6000);
    EXPECT_CALL(impl_, reserve(256, device_, stream_))
        .WillOnce(Return(BufferView{base, 0, 256}));
    EXPECT_CALL(impl_, map(_, device_, context_, stream_)).Times(1).WillOnce(ReturnArg<0>());
    // 最後の releaseChunk 成功時に unmap が1回呼ばれる
    EXPECT_CALL(impl_, unmap(_, _, device_, context_, stream_)).Times(1);

    policy_.initialize(cfg, &resource_);
    auto b = policy_.addChunk(256, 1);

    // incrementPending でカウントアップ
    policy_.incrementPending(b.id);

    // pending > 0 なので解放できない
    EXPECT_FALSE(policy_.releaseChunk(b.id));

    // decrementPending でカウントダウン
    policy_.decrementPending(b.id);

    // pending=0 になったので解放可能
    EXPECT_TRUE(policy_.releaseChunk(b.id));
}

TEST_F(HierarchicalChunkLocatorTest, DecrementPendingDoesNotUnderflow) {
    typename Policy::Config cfg{device_, context_, stream_, {256}, /*initial_bytes=*/256, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0x6100);
    EXPECT_CALL(impl_, reserve(256, device_, stream_))
        .WillOnce(Return(BufferView{base, 0, 256}));
    EXPECT_CALL(impl_, map(_, device_, context_, stream_)).WillRepeatedly(ReturnArg<0>());
    EXPECT_CALL(impl_, unmap(_, _, device_, context_, stream_)).Times(1);

    policy_.initialize(cfg, &resource_);
    auto b = policy_.addChunk(256, 1);

    // 0の状態からdecrementしてもアンダーフローしない
    policy_.decrementPending(b.id);
    policy_.decrementPending(b.id);

    // それでも解放可能
    EXPECT_TRUE(policy_.releaseChunk(b.id));
}

// ============================================================================
// decrementPendingAndUsed のテスト
// ============================================================================

TEST_F(HierarchicalChunkLocatorTest, DecrementPendingAndUsed) {
    typename Policy::Config cfg{device_, context_, stream_, {256}, /*initial_bytes=*/256, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0x7000);
    EXPECT_CALL(impl_, reserve(256, device_, stream_))
        .WillOnce(Return(BufferView{base, 0, 256}));
    EXPECT_CALL(impl_, map(_, device_, context_, stream_)).WillRepeatedly(ReturnArg<0>());
    EXPECT_CALL(impl_, unmap(_, _, device_, context_, stream_)).Times(1);

    policy_.initialize(cfg, &resource_);
    auto b = policy_.addChunk(256, 1);

    policy_.incrementPending(b.id);
    policy_.incrementUsed(b.id);

    // 両方 > 0 なので解放できない
    EXPECT_FALSE(policy_.releaseChunk(b.id));

    // 両方同時にデクリメント
    policy_.decrementPendingAndUsed(b.id);

    // 両方 0 になったので解放可能
    EXPECT_TRUE(policy_.releaseChunk(b.id));
}

TEST_F(HierarchicalChunkLocatorTest, DecrementPendingAndUsedMultipleTimes) {
    typename Policy::Config cfg{device_, context_, stream_, {256}, /*initial_bytes=*/256, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0x7100);
    EXPECT_CALL(impl_, reserve(256, device_, stream_))
        .WillOnce(Return(BufferView{base, 0, 256}));
    EXPECT_CALL(impl_, map(_, device_, context_, stream_)).WillRepeatedly(ReturnArg<0>());
    EXPECT_CALL(impl_, unmap(_, _, device_, context_, stream_)).Times(1);

    policy_.initialize(cfg, &resource_);
    auto b = policy_.addChunk(256, 1);

    policy_.incrementPending(b.id);
    policy_.incrementPending(b.id);
    policy_.incrementUsed(b.id);
    policy_.incrementUsed(b.id);

    policy_.decrementPendingAndUsed(b.id);
    EXPECT_FALSE(policy_.releaseChunk(b.id));

    policy_.decrementPendingAndUsed(b.id);
    EXPECT_TRUE(policy_.releaseChunk(b.id));
}

// ============================================================================
// releaseChunk の失敗ケース（used/pending残存）のテスト
// ============================================================================

TEST_F(HierarchicalChunkLocatorTest, ReleaseChunkFailsWhenUsedRemains) {
    typename Policy::Config cfg{device_, context_, stream_, {256}, /*initial_bytes=*/256, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0x8000);
    EXPECT_CALL(impl_, reserve(256, device_, stream_))
        .WillOnce(Return(BufferView{base, 0, 256}));
    EXPECT_CALL(impl_, map(_, device_, context_, stream_)).WillRepeatedly(ReturnArg<0>());

    policy_.initialize(cfg, &resource_);
    auto b = policy_.addChunk(256, 1);

    policy_.incrementUsed(b.id);
    EXPECT_FALSE(policy_.releaseChunk(b.id));
}

TEST_F(HierarchicalChunkLocatorTest, ReleaseChunkFailsWhenPendingRemains) {
    typename Policy::Config cfg{device_, context_, stream_, {256}, /*initial_bytes=*/256, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0x8100);
    EXPECT_CALL(impl_, reserve(256, device_, stream_))
        .WillOnce(Return(BufferView{base, 0, 256}));
    EXPECT_CALL(impl_, map(_, device_, context_, stream_)).WillRepeatedly(ReturnArg<0>());

    policy_.initialize(cfg, &resource_);
    auto b = policy_.addChunk(256, 1);

    policy_.incrementPending(b.id);
    EXPECT_FALSE(policy_.releaseChunk(b.id));
}

TEST_F(HierarchicalChunkLocatorTest, ReleaseChunkFailsForInvalidState) {
    typename Policy::Config cfg{device_, context_, stream_, {256}, /*initial_bytes=*/256, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0x8200);
    EXPECT_CALL(impl_, reserve(256, device_, stream_))
        .WillOnce(Return(BufferView{base, 0, 256}));
    EXPECT_CALL(impl_, map(_, device_, context_, stream_)).WillRepeatedly(ReturnArg<0>());
    EXPECT_CALL(impl_, unmap(_, _, device_, context_, stream_)).Times(1);

    policy_.initialize(cfg, &resource_);
    auto b = policy_.addChunk(256, 1);

    // 一度解放
    EXPECT_TRUE(policy_.releaseChunk(b.id));

    // 二重解放は失敗
    EXPECT_FALSE(policy_.releaseChunk(b.id));
}

TEST_F(HierarchicalChunkLocatorTest, ReleaseChunkFailsForInvalidId) {
    typename Policy::Config cfg{device_, context_, stream_, {256}, /*initial_bytes=*/256, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0x8300);
    EXPECT_CALL(impl_, reserve(256, device_, stream_))
        .WillOnce(Return(BufferView{base, 0, 256}));

    policy_.initialize(cfg, &resource_);

    // 存在しないID
    BufferId invalid{99999};
    EXPECT_FALSE(policy_.releaseChunk(invalid));
}

// ============================================================================
// pickLayer（サイズクラス選択）のテスト
// ============================================================================

TEST_F(HierarchicalChunkLocatorTest, PickLayerSelectsSmallestSufficientLayer) {
    typename Policy::Config cfg{device_, context_, stream_, {1024, 512, 256, 128}, /*initial_bytes=*/1024, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0x9000);
    EXPECT_CALL(impl_, reserve(1024, device_, stream_))
        .WillRepeatedly(Return(BufferView{base, 0, 1024}));
    EXPECT_CALL(impl_, map(_, device_, context_, stream_)).WillRepeatedly(ReturnArg<0>());

    policy_.initialize(cfg, &resource_);

    // 64バイト要求 -> 128バイト層（最小で足りる層）
    auto b1 = policy_.addChunk(64, 1);
    EXPECT_EQ(policy_.findChunkSize(b1.id), 128);

    // 128バイト要求 -> 128バイト層
    auto b2 = policy_.addChunk(128, 1);
    EXPECT_EQ(policy_.findChunkSize(b2.id), 128);

    // 129バイト要求 -> 256バイト層
    auto b3 = policy_.addChunk(129, 1);
    EXPECT_EQ(policy_.findChunkSize(b3.id), 256);

    // 512バイト要求 -> 512バイト層
    auto b4 = policy_.addChunk(512, 1);
    EXPECT_EQ(policy_.findChunkSize(b4.id), 512);

    // 513バイト要求 -> 1024バイト層
    auto b5 = policy_.addChunk(513, 1);
    EXPECT_EQ(policy_.findChunkSize(b5.id), 1024);
}

TEST_F(HierarchicalChunkLocatorTest, AddChunkFailsWhenRequestExceedsAllLayers) {
    typename Policy::Config cfg{device_, context_, stream_, {256, 128}, /*initial_bytes=*/256, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0x9100);
    EXPECT_CALL(impl_, reserve(256, device_, stream_))
        .WillOnce(Return(BufferView{base, 0, 256}));

    policy_.initialize(cfg, &resource_);

    // 最大層よりも大きいサイズを要求
    orteaf::tests::ExpectError(::orteaf::internal::diagnostics::error::OrteafErrc::OutOfMemory,
                               [&] { policy_.addChunk(512, 1); });
}

// ============================================================================
// region_multiplier の動作テスト
// ============================================================================

TEST_F(HierarchicalChunkLocatorTest, RegionMultiplierAllocatesMultipleChunks) {
    // initial_bytes=0 と region_multiplier=4 の場合:
    // initial_bytes=0 なら levels[0]=256 が initial になり、初期化時に 256 バイト確保。
    // その後 ensureFreeSlot で addRegion が呼ばれる際に region_multiplier が適用される。
    typename Policy::Config cfg{device_, context_, stream_, {256}, /*initial_bytes=*/0, /*region_multiplier=*/4};

    void* base = reinterpret_cast<void*>(0xA000);
    // initial_bytes=0 時は levels[0]=256 が initial になる
    // その後 addRegion は chunk_size * region_multiplier = 256 * 4 = 1024
    EXPECT_CALL(impl_, reserve(_, device_, stream_))
        .WillRepeatedly(Return(BufferView{base, 0, 1024}));
    EXPECT_CALL(impl_, map(_, device_, context_, stream_)).WillRepeatedly(ReturnArg<0>());

    policy_.initialize(cfg, &resource_);

    // チャンクを割り当て
    auto b1 = policy_.addChunk(256, 1);
    auto b2 = policy_.addChunk(256, 1);
    auto b3 = policy_.addChunk(256, 1);
    auto b4 = policy_.addChunk(256, 1);

    EXPECT_NE(b1.view.data(), nullptr);
    EXPECT_NE(b2.view.data(), nullptr);
    EXPECT_NE(b3.view.data(), nullptr);
    EXPECT_NE(b4.view.data(), nullptr);
}

TEST_F(HierarchicalChunkLocatorTest, RegionMultiplierZeroDefaultsToOne) {
    typename Policy::Config cfg{device_, context_, stream_, {256}, /*initial_bytes=*/0, /*region_multiplier=*/0};

    void* base = reinterpret_cast<void*>(0xA100);
    // region_multiplier=0 は 1 にデフォルトされるので 256 バイト確保
    EXPECT_CALL(impl_, reserve(256, device_, stream_))
        .WillOnce(Return(BufferView{base, 0, 256}));
    EXPECT_CALL(impl_, map(_, device_, context_, stream_)).WillRepeatedly(ReturnArg<0>());

    policy_.initialize(cfg, &resource_);
    auto b1 = policy_.addChunk(256, 1);
    EXPECT_NE(b1.view.data(), nullptr);
}

// ============================================================================
// 無効なIDでの操作テスト
// ============================================================================

TEST_F(HierarchicalChunkLocatorTest, IncrementUsedOnInvalidIdDoesNothing) {
    typename Policy::Config cfg{device_, context_, stream_, {256}, /*initial_bytes=*/256, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0xB000);
    EXPECT_CALL(impl_, reserve(256, device_, stream_))
        .WillOnce(Return(BufferView{base, 0, 256}));

    policy_.initialize(cfg, &resource_);

    // 無効なIDでも例外を投げない（何もしない）
    BufferId invalid{99999};
    policy_.incrementUsed(invalid);
    policy_.decrementUsed(invalid);
    policy_.incrementPending(invalid);
    policy_.decrementPending(invalid);
    policy_.decrementPendingAndUsed(invalid);
}

TEST_F(HierarchicalChunkLocatorTest, OperationsOnFreedSlotDoNothing) {
    typename Policy::Config cfg{device_, context_, stream_, {256}, /*initial_bytes=*/256, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0xB100);
    EXPECT_CALL(impl_, reserve(256, device_, stream_))
        .WillOnce(Return(BufferView{base, 0, 256}));
    EXPECT_CALL(impl_, map(_, device_, context_, stream_)).WillRepeatedly(ReturnArg<0>());
    EXPECT_CALL(impl_, unmap(_, _, device_, context_, stream_)).Times(1);

    policy_.initialize(cfg, &resource_);
    auto b = policy_.addChunk(256, 1);
    BufferId id = b.id;

    EXPECT_TRUE(policy_.releaseChunk(id));

    // 解放後のIDに対する操作は何もしない（findSlotがnullptrを返す）
    policy_.incrementUsed(id);
    policy_.decrementUsed(id);
    policy_.incrementPending(id);
    policy_.decrementPending(id);
}

// ============================================================================
// 空のレベル設定での動作テスト
// ============================================================================

TEST_F(HierarchicalChunkLocatorTest, EmptyLevelsWithZeroInitialBytes) {
    typename Policy::Config cfg{device_, context_, stream_, {}, /*initial_bytes=*/0, /*region_multiplier=*/1};

    // 空のlevelsでは初期確保は行われない
    policy_.initialize(cfg, &resource_);

    // 割り当て要求は失敗する（適切な層がない）
    orteaf::tests::ExpectError(::orteaf::internal::diagnostics::error::OrteafErrc::OutOfMemory,
                               [&] { policy_.addChunk(64, 1); });
}

// ============================================================================
// snapshot のデバッグ機能テスト
// ============================================================================

#if ORTEAF_CORE_DEBUG_ENABLED
TEST_F(HierarchicalChunkLocatorTest, SnapshotReturnsCorrectState) {
    typename Policy::Config cfg{device_, context_, stream_, {512, 256}, /*initial_bytes=*/512, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0xC000);
    EXPECT_CALL(impl_, reserve(512, device_, stream_))
        .WillOnce(Return(BufferView{base, 0, 512}));
    EXPECT_CALL(impl_, map(_, device_, context_, stream_)).WillRepeatedly(ReturnArg<0>());

    policy_.initialize(cfg, &resource_);

    auto snap1 = policy_.snapshot();
    EXPECT_EQ(snap1.layers.size(), 2);
    EXPECT_EQ(snap1.layers[0].chunk_size, 512);
    EXPECT_EQ(snap1.layers[1].chunk_size, 256);
    EXPECT_EQ(snap1.layers[0].slots.size(), 1);
    EXPECT_EQ(snap1.layers[0].free_list.size(), 1);

    // チャンク割り当て後
    auto b = policy_.addChunk(256, 1);
    auto snap2 = policy_.snapshot();

    // ルート層はsplitされた
    EXPECT_EQ(snap2.layers[0].slots[0].state, Policy::State::Split);
    // 子層にスロットが追加された
    EXPECT_GT(snap2.layers[1].slots.size(), 0);
}

TEST_F(HierarchicalChunkLocatorTest, ValidatePassesOnValidState) {
    typename Policy::Config cfg{device_, context_, stream_, {512, 256, 128}, /*initial_bytes=*/512, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0xC100);
    EXPECT_CALL(impl_, reserve(512, device_, stream_))
        .WillOnce(Return(BufferView{base, 0, 512}));
    // 128バイトチャンク2つを割り当てるために map が2回呼ばれる
    EXPECT_CALL(impl_, map(_, device_, context_, stream_)).Times(2).WillRepeatedly(ReturnArg<0>());
    // 2つのチャンクを解放するので unmap が2回呼ばれる
    EXPECT_CALL(impl_, unmap(_, _, device_, context_, stream_)).Times(2);

    policy_.initialize(cfg, &resource_);

    auto b1 = policy_.addChunk(128, 1);
    auto b2 = policy_.addChunk(128, 1);

    // validateは例外を投げない
    EXPECT_NO_THROW(policy_.validate());

    policy_.releaseChunk(b1.id);
    policy_.releaseChunk(b2.id);

    EXPECT_NO_THROW(policy_.validate());
}
#endif

// ============================================================================
// 複数リージョン追加のテスト
// ============================================================================

TEST_F(HierarchicalChunkLocatorTest, MultipleRegionsAreAddedWhenExhausted) {
    typename Policy::Config cfg{device_, context_, stream_, {256}, /*initial_bytes=*/256, /*region_multiplier=*/1};

    void* base1 = reinterpret_cast<void*>(0xD000);
    void* base2 = reinterpret_cast<void*>(0xD100);
    EXPECT_CALL(impl_, reserve(256, device_, stream_))
        .WillOnce(Return(BufferView{base1, 0, 256}))
        .WillOnce(Return(BufferView{base2, 0, 256}));
    EXPECT_CALL(impl_, map(_, device_, context_, stream_)).WillRepeatedly(ReturnArg<0>());

    policy_.initialize(cfg, &resource_);

    // 最初のリージョンから割り当て
    auto b1 = policy_.addChunk(256, 1);
    EXPECT_EQ(b1.view.data(), base1);

    // 追加リージョンが必要
    auto b2 = policy_.addChunk(256, 1);
    EXPECT_EQ(b2.view.data(), base2);
}

// ============================================================================
// エッジケース（境界値）のテスト
// ============================================================================

TEST_F(HierarchicalChunkLocatorTest, ZeroSizeAllocationUsesSmallestLayer) {
    typename Policy::Config cfg{device_, context_, stream_, {256, 128, 64}, /*initial_bytes=*/256, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0xE000);
    EXPECT_CALL(impl_, reserve(256, device_, stream_))
        .WillOnce(Return(BufferView{base, 0, 256}));
    EXPECT_CALL(impl_, map(_, device_, context_, stream_)).WillRepeatedly(ReturnArg<0>());

    policy_.initialize(cfg, &resource_);

    // 0バイト要求でも最小層から割り当て
    auto b = policy_.addChunk(0, 1);
    EXPECT_EQ(policy_.findChunkSize(b.id), 64);
}

TEST_F(HierarchicalChunkLocatorTest, ExactSizeMatchSelectsCorrectLayer) {
    typename Policy::Config cfg{device_, context_, stream_, {256, 128}, /*initial_bytes=*/256, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0xE100);
    EXPECT_CALL(impl_, reserve(256, device_, stream_))
        .WillRepeatedly(Return(BufferView{base, 0, 256}));
    EXPECT_CALL(impl_, map(_, device_, context_, stream_)).WillRepeatedly(ReturnArg<0>());

    policy_.initialize(cfg, &resource_);

    // ちょうど128バイト
    auto b1 = policy_.addChunk(128, 1);
    EXPECT_EQ(policy_.findChunkSize(b1.id), 128);

    // ちょうど256バイト
    auto b2 = policy_.addChunk(256, 1);
    EXPECT_EQ(policy_.findChunkSize(b2.id), 256);
}

TEST_F(HierarchicalChunkLocatorTest, SingleLayerConfiguration) {
    typename Policy::Config cfg{device_, context_, stream_, {1024}, /*initial_bytes=*/1024, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0xE200);
    EXPECT_CALL(impl_, reserve(1024, device_, stream_))
        .WillOnce(Return(BufferView{base, 0, 1024}));
    EXPECT_CALL(impl_, map(_, device_, context_, stream_)).WillRepeatedly(ReturnArg<0>());
    EXPECT_CALL(impl_, unmap(_, _, device_, context_, stream_)).Times(1);

    policy_.initialize(cfg, &resource_);

    auto b = policy_.addChunk(512, 1);
    EXPECT_EQ(policy_.findChunkSize(b.id), 1024);
    EXPECT_TRUE(policy_.releaseChunk(b.id));
}

// ============================================================================
// 複雑なSplit/Mergeシナリオのテスト
// ============================================================================

TEST_F(HierarchicalChunkLocatorTest, DeepSplitAcrossMultipleLayers) {
    typename Policy::Config cfg{device_, context_, stream_, {1024, 512, 256, 128}, /*initial_bytes=*/1024, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0xF000);
    EXPECT_CALL(impl_, reserve(1024, device_, stream_))
        .WillOnce(Return(BufferView{base, 0, 1024}));
    // 8つの128バイトチャンクを割り当てるために map が8回呼ばれる
    EXPECT_CALL(impl_, map(_, device_, context_, stream_)).Times(8).WillRepeatedly(ReturnArg<0>());
    // 8つのチャンクを解放するので unmap が8回呼ばれる
    EXPECT_CALL(impl_, unmap(_, _, device_, context_, stream_)).Times(8);

    policy_.initialize(cfg, &resource_);

    // 最小層から8つ割り当て（1024 -> 512x2 -> 256x4 -> 128x8）
    std::vector<allocator::MemoryBlock<Backend::Cpu>> blocks;
    for (int i = 0; i < 8; ++i) {
        blocks.push_back(policy_.addChunk(128, 1));
        EXPECT_EQ(policy_.findChunkSize(blocks.back().id), 128);
    }

    // すべて解放
    for (auto& b : blocks) {
        EXPECT_TRUE(policy_.releaseChunk(b.id));
    }

#if ORTEAF_CORE_DEBUG_ENABLED
    policy_.validate();
#endif
}

TEST_F(HierarchicalChunkLocatorTest, PartialMergeDoesNotOccur) {
    typename Policy::Config cfg{device_, context_, stream_, {512, 256}, /*initial_bytes=*/512, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0xF100);
    EXPECT_CALL(impl_, reserve(512, device_, stream_))
        .WillOnce(Return(BufferView{base, 0, 512}));
    // 2つの256バイトチャンクを割り当てるために map が2回呼ばれる
    EXPECT_CALL(impl_, map(_, device_, context_, stream_)).Times(2).WillRepeatedly(ReturnArg<0>());
    // 2つのチャンクを解放するので unmap が2回呼ばれる
    EXPECT_CALL(impl_, unmap(_, _, device_, context_, stream_)).Times(2);

    policy_.initialize(cfg, &resource_);

    // 2つの256バイトチャンクを割り当て
    auto b1 = policy_.addChunk(256, 1);
    auto b2 = policy_.addChunk(256, 1);

    // 1つだけ解放しても親はマージされない
    EXPECT_TRUE(policy_.releaseChunk(b1.id));

#if ORTEAF_CORE_DEBUG_ENABLED
    auto snap = policy_.snapshot();
    // ルート層はまだSplit状態
    EXPECT_EQ(snap.layers[0].slots[0].state, Policy::State::Split);
#endif

    // 残りを解放
    EXPECT_TRUE(policy_.releaseChunk(b2.id));

#if ORTEAF_CORE_DEBUG_ENABLED
    auto snap2 = policy_.snapshot();
    // すべて解放後、ルート層はFreeに戻る
    EXPECT_EQ(snap2.layers[0].slots[0].state, Policy::State::Free);
#endif
}

TEST_F(HierarchicalChunkLocatorTest, AlternatingAllocFreePattern) {
    typename Policy::Config cfg{device_, context_, stream_, {256, 128}, /*initial_bytes=*/256, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0xF200);
    EXPECT_CALL(impl_, reserve(256, device_, stream_))
        .WillOnce(Return(BufferView{base, 0, 256}));
    // 10回の割り当て・解放を繰り返すので map/unmap が各10回呼ばれる
    EXPECT_CALL(impl_, map(_, device_, context_, stream_)).Times(10).WillRepeatedly(ReturnArg<0>());
    EXPECT_CALL(impl_, unmap(_, _, device_, context_, stream_)).Times(10);

    policy_.initialize(cfg, &resource_);

    // 交互に割り当て・解放を繰り返す
    for (int i = 0; i < 10; ++i) {
        auto b = policy_.addChunk(128, 1);
        EXPECT_TRUE(policy_.releaseChunk(b.id));
    }

#if ORTEAF_CORE_DEBUG_ENABLED
    policy_.validate();
#endif
}

// ============================================================================
// 並行性のテスト（スレッドセーフ）
// ============================================================================

TEST_F(HierarchicalChunkLocatorTest, ConcurrentIncrementDecrement) {
    typename Policy::Config cfg{device_, context_, stream_, {256}, /*initial_bytes=*/256, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0x10000);
    EXPECT_CALL(impl_, reserve(256, device_, stream_))
        .WillOnce(Return(BufferView{base, 0, 256}));
    // チャンク1つを割り当てるために map が1回呼ばれる
    EXPECT_CALL(impl_, map(_, device_, context_, stream_)).Times(1).WillOnce(ReturnArg<0>());
    // 最後に releaseChunk で unmap が1回呼ばれる
    EXPECT_CALL(impl_, unmap(_, _, device_, context_, stream_)).Times(1);

    policy_.initialize(cfg, &resource_);
    auto b = policy_.addChunk(256, 1);

    constexpr int kIterations = 100;
    std::vector<std::thread> threads;

    // 複数スレッドから同時にincrement/decrement
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([this, &b]() {
            for (int j = 0; j < kIterations; ++j) {
                policy_.incrementUsed(b.id);
                policy_.incrementPending(b.id);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    threads.clear();

    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([this, &b]() {
            for (int j = 0; j < kIterations; ++j) {
                policy_.decrementUsed(b.id);
                policy_.decrementPending(b.id);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // 最終的にカウントは0になっているはず
    EXPECT_TRUE(policy_.releaseChunk(b.id));
}

// ============================================================================
// initial_bytes の動作テスト
// ============================================================================

TEST_F(HierarchicalChunkLocatorTest, InitialBytesLargerThanChunkSize) {
    typename Policy::Config cfg{device_, context_, stream_, {256}, /*initial_bytes=*/1024, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0x11000);
    EXPECT_CALL(impl_, reserve(1024, device_, stream_))
        .WillOnce(Return(BufferView{base, 0, 1024}));
    EXPECT_CALL(impl_, map(_, device_, context_, stream_)).WillRepeatedly(ReturnArg<0>());

    policy_.initialize(cfg, &resource_);

    // 4つのチャンクが初期確保される（1024 / 256 = 4）
    auto b1 = policy_.addChunk(256, 1);
    auto b2 = policy_.addChunk(256, 1);
    auto b3 = policy_.addChunk(256, 1);
    auto b4 = policy_.addChunk(256, 1);

    EXPECT_NE(b1.view.data(), nullptr);
    EXPECT_NE(b2.view.data(), nullptr);
    EXPECT_NE(b3.view.data(), nullptr);
    EXPECT_NE(b4.view.data(), nullptr);
}

TEST_F(HierarchicalChunkLocatorTest, InitialBytesZeroUsesChunkSize) {
    typename Policy::Config cfg{device_, context_, stream_, {512}, /*initial_bytes=*/0, /*region_multiplier=*/1};

    void* base = reinterpret_cast<void*>(0x11100);
    // initial_bytes=0 の場合、最大チャンクサイズ（512）が使われる
    EXPECT_CALL(impl_, reserve(512, device_, stream_))
        .WillOnce(Return(BufferView{base, 0, 512}));
    EXPECT_CALL(impl_, map(_, device_, context_, stream_)).WillRepeatedly(ReturnArg<0>());

    policy_.initialize(cfg, &resource_);

    auto b = policy_.addChunk(512, 1);
    EXPECT_NE(b.view.data(), nullptr);
}

}  // namespace

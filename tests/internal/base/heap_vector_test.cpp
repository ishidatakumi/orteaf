/**
 * @file
 * @brief HeapVectorの全APIと例外安全性を網羅的に検証するユニットテスト。
 *
 * - 基本状態 (`DefaultConstructedState`) と push/emplace/resize/reserve/shrinkToFit など標準操作。
 * - コピー/ムーブ構築・代入、`clear` の破棄挙動、容量制御の確認。
 * - 例外を投げるペイロードを用いた emplace/resize 時の強い例外保証とリーク防止の検証。
 */
#include "orteaf/internal/base/heap_vector.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace orteaf::internal::base {
namespace {

struct CountingPayload {
    static inline int live_instances = 0;
    static inline int copies = 0;
    int value = 0;

    CountingPayload() : value(0) { ++live_instances; }
    explicit CountingPayload(int v) : value(v) { ++live_instances; }
    CountingPayload(const CountingPayload& other) : value(other.value) {
        ++live_instances;
        ++copies;
    }
    CountingPayload(CountingPayload&& other) noexcept : value(other.value) {
        ++live_instances;
    }
    CountingPayload& operator=(const CountingPayload&) = default;
    CountingPayload& operator=(CountingPayload&&) = default;
    ~CountingPayload() { --live_instances; }

    static void ResetCounters() {
        live_instances = 0;
        copies = 0;
    }
};

struct ThrowingPayload {
    static inline int throws_before = 0;
    static inline int live_instances = 0;

    ThrowingPayload() {
        if (throws_before == 0) {
            throw std::runtime_error("ctor failure");
        }
        --throws_before;
        ++live_instances;
    }
    ThrowingPayload(const ThrowingPayload&) {
        if (throws_before == 0) {
            throw std::runtime_error("copy failure");
        }
        --throws_before;
        ++live_instances;
    }
    ~ThrowingPayload() { --live_instances; }
};

}  // namespace

/** @test HeapVectorTest.DefaultConstructedState
 *  @brief Verifies default construction leaves size zero, capacity zero, and null data pointer.
 */
TEST(HeapVectorTest, DefaultConstructedState) {
    HeapVector<int> vec;
    EXPECT_EQ(vec.size(), 0u);
    EXPECT_EQ(vec.capacity(), 0u);
    EXPECT_TRUE(vec.empty());
    EXPECT_EQ(vec.data(), nullptr);
}

/** @test HeapVectorTest.PushAndEmplaceIncreaseSize
 *  @brief Checks that pushBack/emplaceBack append elements in order.
 */
TEST(HeapVectorTest, PushAndEmplaceIncreaseSize) {
    HeapVector<int> vec;
    vec.pushBack(1);
    vec.pushBack(2);
    vec.emplaceBack(3);

    ASSERT_EQ(vec.size(), 3u);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 2);
    EXPECT_EQ(vec[2], 3);
}

/** @test HeapVectorTest.ReserveAndResizeManageCapacity
 *  @brief Validates reserve/resize grow and shrink capacity while initializing elements correctly.
 */
TEST(HeapVectorTest, ReserveAndResizeManageCapacity) {
    HeapVector<int> vec;
    vec.reserve(8);
    EXPECT_GE(vec.capacity(), 8u);
    vec.resize(5);
    EXPECT_EQ(vec.size(), 5u);
    vec.resize(2);
    EXPECT_EQ(vec.size(), 2u);
    vec.resize(4, 9);
    EXPECT_EQ(vec.size(), 4u);
    EXPECT_EQ(vec[3], 9);
}

/** @test HeapVectorTest.ShrinkToFitReleasesExtraCapacity
 *  @brief Ensures shrinkToFit drops excess capacity while preserving stored values.
 */
TEST(HeapVectorTest, ShrinkToFitReleasesExtraCapacity) {
    HeapVector<int> vec;
    vec.reserve(16);
    vec.resize(4, 7);
    const std::size_t before = vec.capacity();
    vec.shrinkToFit();
    EXPECT_LE(vec.capacity(), before);
    EXPECT_EQ(vec.size(), 4u);
    for (int value : {vec[0], vec[1], vec[2], vec[3]}) {
        EXPECT_EQ(value, 7);
    }
}

/** @test HeapVectorTest.CopyConstructionAndAssignmentDuplicateContents
 *  @brief Checks copy constructors/assignments duplicate elements and match live instance counts.
 */
TEST(HeapVectorTest, CopyConstructionAndAssignmentDuplicateContents) {
    CountingPayload::ResetCounters();
    {
        HeapVector<CountingPayload> original;
        original.emplaceBack(CountingPayload{1});
        original.emplaceBack(CountingPayload{2});

        HeapVector<CountingPayload> copy(original);
        EXPECT_EQ(copy.size(), original.size());
        EXPECT_EQ(CountingPayload::live_instances, 4);  // 2 original + 2 copy

        HeapVector<CountingPayload> assigned;
        assigned = copy;
        EXPECT_EQ(assigned.size(), copy.size());
        EXPECT_EQ(CountingPayload::copies, 4);
    }
    EXPECT_EQ(CountingPayload::live_instances, 0);
}

/** @test HeapVectorTest.MoveConstructionAndAssignmentTransferOwnership
 *  @brief Verifies move constructors/assignments transfer ownership leaving the source empty.
 */
TEST(HeapVectorTest, MoveConstructionAndAssignmentTransferOwnership) {
    HeapVector<int> original;
    for (int i = 0; i < 3; ++i) {
        original.pushBack(i);
    }

    const std::size_t capacity_before = original.capacity();
    HeapVector<int> moved(std::move(original));
    EXPECT_EQ(moved.size(), 3u);
    EXPECT_EQ(moved.capacity(), capacity_before);
    EXPECT_EQ(original.size(), 0u);

    HeapVector<int> assigned;
    assigned = std::move(moved);
    EXPECT_EQ(assigned.size(), 3u);
    EXPECT_EQ(moved.size(), 0u);
}

/** @test HeapVectorTest.ClearDestroysAllElements
 *  @brief Confirms clear destroys all elements and resets size to zero.
 */
TEST(HeapVectorTest, ClearDestroysAllElements) {
    CountingPayload::ResetCounters();
    HeapVector<CountingPayload> vec;
    vec.emplaceBack(CountingPayload{});
    vec.emplaceBack(CountingPayload{});
    ASSERT_EQ(CountingPayload::live_instances, 2);
    vec.clear();
    EXPECT_EQ(CountingPayload::live_instances, 0);
    EXPECT_EQ(vec.size(), 0u);
}

/** @test HeapVectorTest.EmplaceBackStrongExceptionGuarantee
 *  @brief Ensures emplaceBack maintains strong exception guarantee when element construction throws.
 */
TEST(HeapVectorTest, EmplaceBackStrongExceptionGuarantee) {
    HeapVector<ThrowingPayload> vec;
    ThrowingPayload::throws_before = 0;
    EXPECT_THROW(vec.emplaceBack(), std::runtime_error);
    EXPECT_EQ(vec.size(), 0u);
    EXPECT_EQ(ThrowingPayload::live_instances, 0);
}

/** @test HeapVectorTest.ResizeConstructorFailureLeavesVectorUnchanged
 *  @brief Verifies resize exceptions rollback vector state without leaking instances.
 */
TEST(HeapVectorTest, ResizeConstructorFailureLeavesVectorUnchanged) {
    HeapVector<ThrowingPayload> vec;
    ThrowingPayload::throws_before = 1;  // first element succeeds, second throws
    EXPECT_THROW(vec.resize(2), std::runtime_error);
    EXPECT_EQ(vec.size(), 0u);
    EXPECT_EQ(ThrowingPayload::live_instances, 0);
}

}  // namespace orteaf::internal::base

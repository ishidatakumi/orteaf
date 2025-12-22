/**
 * @file
 * @brief BlockVectorとRuntimeBlockVectorのAPIと安定アドレス性を検証するテスト。
 */
#include "orteaf/internal/base/block_vector.h"
#include "orteaf/internal/base/runtime_block_vector.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <stdexcept>

namespace orteaf::internal::base {
namespace {

struct CountingPayload {
    static inline int live_instances = 0;
    int value = 0;

    CountingPayload() : value(0) { ++live_instances; }
    explicit CountingPayload(int v) : value(v) { ++live_instances; }
    CountingPayload(const CountingPayload& other) : value(other.value) { ++live_instances; }
    CountingPayload(CountingPayload&& other) noexcept : value(other.value) { ++live_instances; }
    ~CountingPayload() { --live_instances; }

    CountingPayload& operator=(const CountingPayload&) = default;
    CountingPayload& operator=(CountingPayload&&) = default;

    static void Reset() { live_instances = 0; }
};

}  // namespace

TEST(BlockVectorTest, BasicOperations) {
    BlockVector<int, 2> vec;
    EXPECT_TRUE(vec.empty());
    vec.pushBack(1);
    vec.emplaceBack(2);
    vec.pushBack(3);
    EXPECT_EQ(vec.size(), 3u);
    EXPECT_EQ(vec.front(), 1);
    EXPECT_EQ(vec.back(), 3);
    EXPECT_EQ(vec.at(1), 2);
    EXPECT_THROW(vec.at(3), std::out_of_range);

    int sum = 0;
    for (int v : vec) sum += v;
    EXPECT_EQ(sum, 6);

    vec.popBack();
    EXPECT_EQ(vec.size(), 2u);
}

TEST(BlockVectorTest, AddressStabilityAcrossGrow) {
    BlockVector<CountingPayload, 2> vec;
    CountingPayload::Reset();
    vec.emplaceBack(1);
    vec.emplaceBack(2);
    CountingPayload* first_ptr = &vec[0];
    const std::size_t capacity_before = vec.capacity();
    vec.emplaceBack(3);
    EXPECT_EQ(&vec[0], first_ptr);
    EXPECT_GE(vec.capacity(), capacity_before);
}

TEST(RuntimeBlockVectorTest, BasicOperations) {
    RuntimeBlockVector<int> vec(3);
    EXPECT_EQ(vec.blockSize(), 3u);
    vec.pushBack(4);
    vec.emplaceBack(5);
    vec.pushBack(6);
    EXPECT_EQ(vec.size(), 3u);
    EXPECT_EQ(vec.front(), 4);
    EXPECT_EQ(vec.back(), 6);
    EXPECT_EQ(vec.at(1), 5);
    EXPECT_THROW(vec.at(3), std::out_of_range);

    int sum = 0;
    for (int v : vec) sum += v;
    EXPECT_EQ(sum, 15);
}

TEST(RuntimeBlockVectorTest, AddressStabilityAcrossGrow) {
    RuntimeBlockVector<CountingPayload> vec(2);
    CountingPayload::Reset();
    vec.emplaceBack(1);
    vec.emplaceBack(2);
    CountingPayload* first_ptr = &vec[0];
    const std::size_t capacity_before = vec.capacity();
    vec.emplaceBack(3);
    EXPECT_EQ(&vec[0], first_ptr);
    EXPECT_GE(vec.capacity(), capacity_before);
}

TEST(RuntimeBlockVectorTest, RejectsZeroBlockSize) {
    EXPECT_THROW(RuntimeBlockVector<int> vec(0), std::invalid_argument);
}

}  // namespace orteaf::internal::base

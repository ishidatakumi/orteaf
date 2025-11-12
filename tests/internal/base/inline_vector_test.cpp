/**
 * @file
 * @brief Unit tests that verify the basic behaviors of `InlineVector`.
 *
 * - `DefaultConstructedStateIsZeroed`: checks that a default-constructed instance is size zero and all elements are zero-initialized.
 * - `ManualWritesStayWithinCapacity`: ensures manual writes stay within capacity and size tracking reflects the writes.
 * - `CopyConstructionCopiesBufferAndSize`: validates that copy construction duplicates the buffer and size without aliasing.
 */
#include "orteaf/internal/base/inline_vector.h"

#include <algorithm>
#include <array>
#include <gtest/gtest.h>

namespace orteaf::internal::base {

/**
 * @test InlineVectorTest.DefaultConstructedStateIsZeroed
 * @brief Verifies that default construction yields size zero and zero-initialized elements.
 */
TEST(InlineVectorTest, DefaultConstructedStateIsZeroed) {
    InlineVector<int, 4> vec;
    EXPECT_EQ(vec.size, 0u);
    for (int value : vec.data) {
        EXPECT_EQ(value, 0);
    }
}

/**
 * @test InlineVectorTest.ManualWritesStayWithinCapacity
 * @brief Confirms manual writes remain within capacity and size updates are reflected.
 */
TEST(InlineVectorTest, ManualWritesStayWithinCapacity) {
    InlineVector<int, 3> vec;
    vec.data[0] = 1;
    vec.data[1] = 2;
    vec.data[2] = 3;
    vec.size = 3;

    EXPECT_EQ(vec.size, 3u);
    EXPECT_EQ(vec.data[0], 1);
    EXPECT_EQ(vec.data[1], 2);
    EXPECT_EQ(vec.data[2], 3);
}

/**
 * @test InlineVectorTest.CopyConstructionCopiesBufferAndSize
 * @brief Ensures copy construction duplicates the buffer and size independently.
 */
TEST(InlineVectorTest, CopyConstructionCopiesBufferAndSize) {
    InlineVector<int, 2> original;
    original.data[0] = 7;
    original.size = 1;

    InlineVector<int, 2> copy = original;
    EXPECT_EQ(copy.size, 1u);
    EXPECT_EQ(copy.data[0], 7);

    original.data[0] = 9;
    EXPECT_EQ(copy.data[0], 7) << "copy should not alias";
}

}  // namespace orteaf::internal::base

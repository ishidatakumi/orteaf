#include "orteaf/internal/base/math_utils.h"

#include <gtest/gtest.h>

#include <array>
#include <limits>

namespace base = orteaf::internal::base;

TEST(MathUtils, IsPowerOfTwoBasicCases) {
    std::array<std::size_t, 10> powers{
        1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u, 512u,
    };

    for (auto value : powers) {
        EXPECT_TRUE(base::isPowerOfTwo(value)) << value;
    }

    std::array<std::size_t, 6> non_powers{
        0u, 3u, 5u, 12u, 63u, 511u,
    };
    for (auto value : non_powers) {
        EXPECT_FALSE(base::isPowerOfTwo(value)) << value;
    }
}

TEST(MathUtils, IsPowerOfTwoHighestBit) {
    constexpr std::size_t bits = sizeof(std::size_t) * 8;
    const std::size_t highest = std::size_t{1} << (bits - 1);

    EXPECT_TRUE(base::isPowerOfTwo(highest));
    EXPECT_FALSE(base::isPowerOfTwo(highest - 1));
}

TEST(MathUtils, NextPowerOfTwoBasicCases) {
    EXPECT_EQ(base::nextPowerOfTwo(0), 1u);
    EXPECT_EQ(base::nextPowerOfTwo(1), 1u);
    EXPECT_EQ(base::nextPowerOfTwo(2), 2u);
    EXPECT_EQ(base::nextPowerOfTwo(3), 4u);
    EXPECT_EQ(base::nextPowerOfTwo(5), 8u);
    EXPECT_EQ(base::nextPowerOfTwo(16), 16u);
    EXPECT_EQ(base::nextPowerOfTwo(17), 32u);
    EXPECT_EQ(base::nextPowerOfTwo(63), 64u);
}

TEST(MathUtils, NextPowerOfTwoMonotonicRange) {
    constexpr std::size_t limit = 1u << 18;  // wide enough for regression detection
    for (std::size_t value = 0; value < limit; ++value) {
        const std::size_t next = base::nextPowerOfTwo(value);
        EXPECT_TRUE(base::isPowerOfTwo(next));
        EXPECT_GE(next, value == 0 ? 1u : value);

        if (next > 1u) {
            EXPECT_LT(next >> 1, value == 0 ? 1u : value + 1u);
        }
    }
}

TEST(MathUtils, NextPowerOfTwoHighestBit) {
    constexpr std::size_t bits = sizeof(std::size_t) * 8;
    const std::size_t highest = std::size_t{1} << (bits - 1);

    EXPECT_EQ(base::nextPowerOfTwo(highest), highest);
    EXPECT_EQ(base::nextPowerOfTwo(highest - 1), highest);
}

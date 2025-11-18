#include "orteaf/internal/base/strong_id.h"

#include <gtest/gtest.h>

namespace base = orteaf::internal::base;

TEST(StrongId, BasicComparisonAndConversion) {
    base::StreamId stream1{3};
    base::StreamId stream2{3};
    base::StreamId stream3{4};

    EXPECT_EQ(stream1, stream2);
    EXPECT_NE(stream1, stream3);
    EXPECT_EQ(static_cast<uint8_t>(stream1), 3u);
    EXPECT_LT(stream1, stream3);
    EXPECT_TRUE(stream1.isValid());
}

TEST(StrongId, InvalidHelper) {
    constexpr auto bad = base::ContextId::invalid();
    EXPECT_FALSE(bad.isValid());
    EXPECT_EQ(static_cast<uint8_t>(bad), static_cast<uint8_t>(~0));
}

TEST(StrongId, DeviceTypeIsIndependent) {
    base::DeviceId device{0};
    base::StreamId stream{0};
    // Ensure there is no implicit conversion between different StrongId tags.
    static_assert(!std::is_convertible_v<base::StreamId, base::DeviceId>);
    static_assert(!std::is_convertible_v<base::DeviceId, base::StreamId>);
    EXPECT_EQ(static_cast<uint32_t>(device), 0u);
    EXPECT_EQ(static_cast<uint8_t>(stream), 0u);
}

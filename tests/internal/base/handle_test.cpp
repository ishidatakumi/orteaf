#include "orteaf/internal/base/handle.h"

#include <gtest/gtest.h>

namespace base = orteaf::internal::base;

TEST(Handle, BasicComparisonAndConversion) {
    base::StreamId stream1{3};
    base::StreamId stream2{3};
    base::StreamId stream3{4};

    EXPECT_EQ(stream1, stream2);
    EXPECT_NE(stream1, stream3);
    EXPECT_EQ(static_cast<uint32_t>(stream1), 3u); // Handle casts to index type (uint32_t)
    EXPECT_LT(stream1, stream3);
    EXPECT_TRUE(stream1.isValid());
}

TEST(Handle, InvalidHelper) {
    constexpr auto bad = base::ContextId::invalid();
    EXPECT_FALSE(bad.isValid());
    EXPECT_EQ(static_cast<uint32_t>(bad), base::ContextId::invalid_index());
}

TEST(Handle, DeviceTypeIsIndependent) {
    base::DeviceId device{0};
    base::StreamId stream{0};
    // Ensure there is no implicit conversion between different Handle tags.
    static_assert(!std::is_convertible_v<base::StreamId, base::DeviceId>);
    static_assert(!std::is_convertible_v<base::DeviceId, base::StreamId>);
    EXPECT_EQ(static_cast<uint32_t>(device), 0u);
    EXPECT_EQ(static_cast<uint32_t>(stream), 0u);
}

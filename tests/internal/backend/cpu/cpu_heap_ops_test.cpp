#include "orteaf/internal/backend/cpu/cpu_heap_ops.h"

#include <gtest/gtest.h>

namespace orteaf::tests {
using orteaf::internal::backend::cpu::CpuHeapOps;

TEST(CpuHeapOpsTest, ReserveZeroReturnsEmpty) {
    auto region = CpuHeapOps::reserve(0);
    EXPECT_FALSE(region);
}

TEST(CpuHeapOpsTest, ReserveMapUnmapRoundTrip) {
    constexpr std::size_t kSize = 4096;
    auto region = CpuHeapOps::reserve(kSize);
    ASSERT_TRUE(region);
    EXPECT_EQ(region.size(), kSize);

    auto mapped = CpuHeapOps::map(region);
    EXPECT_TRUE(mapped);
    EXPECT_EQ(mapped.size(), kSize);

    // Should not throw
    CpuHeapOps::unmap(mapped, kSize);
}

TEST(CpuHeapOpsTest, MapUnmapOnEmptyIsNoOp) {
    CpuHeapOps::map({});          // no-throw
    CpuHeapOps::unmap(CpuHeapOps::BufferView{}, 0);     // no-throw
    SUCCEED();
}

}  // namespace orteaf::tests

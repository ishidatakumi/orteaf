#include "orteaf/internal/execution/cpu/resource/cpu_heap_ops.h"

#include <gtest/gtest.h>

namespace orteaf::tests {
using orteaf::internal::execution::cpu::resource::CpuHeapOps;

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
    CpuHeapOps::unmap(region, kSize);
}

TEST(CpuHeapOpsTest, MapUnmapOnEmptyIsNoOp) {
    CpuHeapOps::map({});          // no-throw
    CpuHeapOps::unmap(CpuHeapOps::HeapRegion{}, 0);     // no-throw
    SUCCEED();
}

}  // namespace orteaf::tests

#include "orteaf/internal/runtime/allocator/resource/cpu/cpu_resource.h"

#include <gtest/gtest.h>

namespace orteaf::tests {
using orteaf::internal::backend::cpu::CpuResource;

TEST(CpuResourceTest, InitializeIsIdempotent) {
    CpuResource::Config cfg{};
    EXPECT_NO_THROW(CpuResource::initialize(cfg));
    EXPECT_NO_THROW(CpuResource::initialize(cfg));  // no-op
}

TEST(CpuResourceTest, AllocateZeroReturnsEmpty) {
    auto view = CpuResource::allocate(0, 64);
    EXPECT_FALSE(view);
}

TEST(CpuResourceTest, AllocateAndDeallocateSucceeds) {
    constexpr std::size_t kSize = 256;
    constexpr std::size_t kAlign = 64;
    auto view = CpuResource::allocate(kSize, kAlign);
    ASSERT_TRUE(view);
    EXPECT_NE(view.data(), nullptr);
    EXPECT_EQ(view.offset(), 0u);
    EXPECT_EQ(view.size(), kSize);

    // Should not throw
    CpuResource::deallocate(view, kSize, kAlign);
}

TEST(CpuResourceTest, DeallocateOnEmptyIsNoOp) {
    CpuResource::deallocate({}, 0, 0);
    SUCCEED();
}

}  // namespace orteaf::tests

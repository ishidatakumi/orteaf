/**
 * @file mps_fence_test.mm
 * @brief Tests for MpsFence behavior.
 */

#include <cstdint>
#include <gtest/gtest.h>

#include "orteaf/internal/execution/mps/resource/mps_fence.h"

namespace mps_res = orteaf::internal::execution::mps::resource;
namespace mps_wrapper = orteaf::internal::execution::mps::platform::wrapper;
namespace base = orteaf::internal::base;

#if ORTEAF_ENABLE_MPS

namespace {
struct FakeFastOpsCompleted {
  static bool isCompleted(mps_wrapper::MpsCommandBuffer_t) { return true; }
};

struct FakeFastOpsNotCompleted {
  static bool isCompleted(mps_wrapper::MpsCommandBuffer_t) { return false; }
};

mps_wrapper::MpsCommandBuffer_t fakeCommandBuffer(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MpsCommandBuffer_t>(value);
}
} // namespace

TEST(MpsFenceResourceTest, DefaultConstructedIsReadyAndCompleted) {
  mps_res::MpsFence fence;
  EXPECT_TRUE(fence.isReady());
  EXPECT_TRUE(fence.isCompleted());
  EXPECT_FALSE(fence.hasFence());
  EXPECT_FALSE(fence.hasCommandBuffer());
  EXPECT_EQ(fence.commandQueueHandle(), base::CommandQueueHandle{});
}

TEST(MpsFenceResourceTest, SetCommandBufferOnlyOnce) {
  mps_res::MpsFence fence;
  auto first = fakeCommandBuffer(0x1);
  auto second = fakeCommandBuffer(0x2);

  EXPECT_TRUE(fence.setCommandBuffer(first));
  EXPECT_EQ(fence.commandBuffer(), first);
  EXPECT_FALSE(fence.setCommandBuffer(second));
  EXPECT_EQ(fence.commandBuffer(), first);
  EXPECT_FALSE(fence.isCompleted());
}

TEST(MpsFenceResourceTest, SetCommandQueueHandleBlockedAfterCommandBuffer) {
  mps_res::MpsFence fence;
  base::CommandQueueHandle handle{7};

  EXPECT_TRUE(fence.setCommandQueueHandle(handle));
  EXPECT_EQ(fence.commandQueueHandle(), handle);
  EXPECT_TRUE(fence.setCommandBuffer(fakeCommandBuffer(0x3)));
  EXPECT_FALSE(fence.setCommandQueueHandle(base::CommandQueueHandle{9}));
  EXPECT_FALSE(fence.setCommandBuffer(fakeCommandBuffer(0x4)));
  EXPECT_FALSE(fence.isCompleted());
  EXPECT_EQ(fence.commandQueueHandle(), handle);
}

TEST(MpsFenceResourceTest, IsReadyUsesFastOpsAndNailsOnCompletion) {
  mps_res::MpsFence fence;
  auto command_buffer = fakeCommandBuffer(0x5);

  ASSERT_TRUE(fence.setCommandBuffer(command_buffer));
  EXPECT_FALSE((fence.isReady<FakeFastOpsNotCompleted>()));
  EXPECT_EQ(fence.commandBuffer(), command_buffer);

  EXPECT_TRUE((fence.isReady<FakeFastOpsCompleted>()));
  EXPECT_TRUE(fence.isReady<FakeFastOpsNotCompleted>());
  EXPECT_EQ(fence.commandBuffer(), nullptr);
  EXPECT_TRUE(fence.isCompleted());
}

#endif // ORTEAF_ENABLE_MPS

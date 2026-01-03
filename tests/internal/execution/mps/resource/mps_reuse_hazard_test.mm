/**
 * @file mps_reuse_hazard_test.mm
 * @brief Tests for MpsReuseHazard behavior.
 */

#include <cstdint>
#include <gtest/gtest.h>

#include "orteaf/internal/execution/mps/resource/mps_reuse_hazard.h"

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

TEST(MpsReuseHazardTest, DefaultConstructedIsCompleted) {
  mps_res::MpsReuseHazard hazard;
  EXPECT_TRUE(hazard.isCompleted());
  EXPECT_FALSE(hazard.hasCommandBuffer());
  EXPECT_EQ(hazard.commandQueueHandle(), base::CommandQueueHandle{});
}

TEST(MpsReuseHazardTest, SetCommandBufferOnlyOnce) {
  mps_res::MpsReuseHazard hazard;
  auto first = fakeCommandBuffer(0x1);
  auto second = fakeCommandBuffer(0x2);

  EXPECT_TRUE(hazard.setCommandBuffer(first));
  EXPECT_EQ(hazard.commandBuffer(), first);
  EXPECT_TRUE(hazard.hasCommandBuffer());
  EXPECT_FALSE(hazard.setCommandBuffer(second));
  EXPECT_EQ(hazard.commandBuffer(), first);
  EXPECT_FALSE(hazard.isCompleted<FakeFastOpsNotCompleted>());
}

TEST(MpsReuseHazardTest, SetCommandQueueHandleBlockedAfterCommandBuffer) {
  mps_res::MpsReuseHazard hazard;
  base::CommandQueueHandle handle{7};

  EXPECT_TRUE(hazard.setCommandQueueHandle(handle));
  EXPECT_EQ(hazard.commandQueueHandle(), handle);
  EXPECT_TRUE(hazard.setCommandBuffer(fakeCommandBuffer(0x3)));
  EXPECT_FALSE(hazard.setCommandQueueHandle(base::CommandQueueHandle{9}));
  EXPECT_FALSE(hazard.setCommandBuffer(fakeCommandBuffer(0x4)));
  EXPECT_FALSE(hazard.isCompleted<FakeFastOpsNotCompleted>());
  EXPECT_EQ(hazard.commandQueueHandle(), handle);
}

TEST(MpsReuseHazardTest, IsCompletedUsesFastOpsAndNailsOnCompletion) {
  mps_res::MpsReuseHazard hazard;
  auto command_buffer = fakeCommandBuffer(0x5);

  ASSERT_TRUE(hazard.setCommandBuffer(command_buffer));
  EXPECT_FALSE((hazard.isCompleted<FakeFastOpsNotCompleted>()));
  EXPECT_EQ(hazard.commandBuffer(), command_buffer);

  EXPECT_TRUE((hazard.isCompleted<FakeFastOpsCompleted>()));
  EXPECT_TRUE(hazard.isCompleted<FakeFastOpsNotCompleted>());
  EXPECT_EQ(hazard.commandBuffer(), nullptr);
  EXPECT_TRUE(hazard.isCompleted());
}

TEST(MpsReuseHazardTest, SetCommandQueueHandleBeforeCommandBuffer) {
  mps_res::MpsReuseHazard hazard;
  base::CommandQueueHandle handle{42};

  EXPECT_TRUE(hazard.setCommandQueueHandle(handle));
  EXPECT_EQ(hazard.commandQueueHandle(), handle);
  EXPECT_TRUE(hazard.setCommandBuffer(fakeCommandBuffer(0x6)));
  EXPECT_TRUE(hazard.hasCommandBuffer());
  EXPECT_EQ(hazard.commandQueueHandle(), handle);
}

#endif // ORTEAF_ENABLE_MPS

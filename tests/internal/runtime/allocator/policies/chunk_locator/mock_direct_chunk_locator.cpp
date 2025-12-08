#include "orteaf/internal/runtime/allocator/policies/chunk_locator/direct_chunk_locator.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/runtime/cpu/resource/cpu_buffer_view.h"
#include "tests/internal/runtime/allocator/testing/mock_resource.h"
#include "tests/internal/testing/error_assert.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace allocator = ::orteaf::internal::runtime::allocator;
namespace policies = ::orteaf::internal::runtime::allocator::policies;
using Backend = ::orteaf::internal::backend::Backend;
using BufferHandle = ::orteaf::internal::base::BufferHandle;
using CpuView = ::orteaf::internal::runtime::cpu::resource::CpuBufferView;
using ::orteaf::internal::runtime::allocator::testing::MockCpuResource;
using ::orteaf::internal::runtime::allocator::testing::MockCpuResourceImpl;

namespace {

using Policy =
    policies::DirectChunkLocatorPolicy<MockCpuResource, Backend::Cpu>;

TEST(DirectChunkLocator, ReleaseChunkCallsResourceWhenFree) {
  Policy policy;
  MockCpuResource resource;
  Policy::Config cfg{};

  NiceMock<MockCpuResourceImpl> impl;
  MockCpuResource::set(&impl);
  policy.initialize(cfg, &resource);

  CpuView view{reinterpret_cast<void *>(0x10), 0, 256};
  EXPECT_CALL(impl, allocate(256, 1)).WillOnce(Return(view));
  EXPECT_CALL(impl, deallocate(view, 256, 1)).Times(1);

  auto block = policy.addChunk(256, 1);
  BufferHandle handle = block.handle;

  EXPECT_TRUE(policy.releaseChunk(handle));
  EXPECT_FALSE(policy.isAlive(handle));

  MockCpuResource::reset();
}

TEST(DirectChunkLocator, InitializeFailsWithNullResource) {
  Policy policy;
  Policy::Config cfg{};

  orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [&] { policy.initialize(cfg, nullptr); });
}

TEST(DirectChunkLocator, AddChunkBeforeInitializeThrows) {
  Policy policy;
  orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
      [&] { policy.addChunk(64, 1); });
}

TEST(DirectChunkLocator, ReleaseChunkSkipsWhenInUse) {
  Policy policy;
  MockCpuResource resource;
  Policy::Config cfg{};

  NiceMock<MockCpuResourceImpl> impl;
  MockCpuResource::set(&impl);
  policy.initialize(cfg, &resource);

  CpuView view{reinterpret_cast<void *>(0x20), 0, 128};
  EXPECT_CALL(impl, allocate(128, 1)).WillOnce(Return(view));
  EXPECT_CALL(impl, deallocate(view, 128, 1)).Times(1);

  auto block = policy.addChunk(128, 1);
  BufferHandle handle = block.handle;
  policy.incrementUsed(handle);

  EXPECT_FALSE(policy.releaseChunk(handle));

  policy.decrementUsed(handle);
  EXPECT_TRUE(policy.releaseChunk(handle));

  MockCpuResource::reset();
}

TEST(DirectChunkLocator, DoubleReleaseFails) {
  Policy policy;
  MockCpuResource resource;
  Policy::Config cfg{};

  NiceMock<MockCpuResourceImpl> impl;
  MockCpuResource::set(&impl);
  policy.initialize(cfg, &resource);

  CpuView view{reinterpret_cast<void *>(0x21), 0, 128};
  EXPECT_CALL(impl, allocate(128, 1)).WillOnce(Return(view));
  EXPECT_CALL(impl, deallocate(view, 128, 1)).Times(1);

  auto block = policy.addChunk(128, 1);
  BufferHandle handle = block.handle;
  EXPECT_TRUE(policy.releaseChunk(handle));
  EXPECT_FALSE(policy.releaseChunk(handle));
  EXPECT_FALSE(policy.isAlive(handle));

  MockCpuResource::reset();
}

TEST(DirectChunkLocator, PendingBlocksPreventRelease) {
  Policy policy;
  MockCpuResource resource;
  Policy::Config cfg{};

  NiceMock<MockCpuResourceImpl> impl;
  MockCpuResource::set(&impl);
  policy.initialize(cfg, &resource);

  CpuView view{reinterpret_cast<void *>(0x30), 0, 64};
  EXPECT_CALL(impl, allocate(64, 1)).WillOnce(Return(view));
  EXPECT_CALL(impl, deallocate(view, 64, 1)).Times(1);

  auto block = policy.addChunk(64, 1);
  BufferHandle handle = block.handle;
  policy.incrementPending(handle);

  EXPECT_FALSE(policy.releaseChunk(handle));

  policy.decrementPending(handle);
  EXPECT_TRUE(policy.releaseChunk(handle));

  MockCpuResource::reset();
}

TEST(DirectChunkLocator, OperationsOnInvalidIdAreNoops) {
  Policy policy;
  MockCpuResource resource;
  Policy::Config cfg{};

  NiceMock<MockCpuResourceImpl> impl;
  MockCpuResource::set(&impl);
  policy.initialize(cfg, &resource);

  BufferHandle invalid{99999};
  policy.incrementUsed(invalid);
  policy.decrementUsed(invalid);
  policy.incrementPending(invalid);
  policy.decrementPending(invalid);
  policy.decrementPendingAndUsed(invalid);
  EXPECT_FALSE(policy.isAlive(invalid));

  MockCpuResource::reset();
}

TEST(DirectChunkLocator, FindChunkSizeReturnsRegisteredValue) {
  Policy policy;
  MockCpuResource resource;
  Policy::Config cfg{};

  NiceMock<MockCpuResourceImpl> impl;
  MockCpuResource::set(&impl);
  policy.initialize(cfg, &resource);

  CpuView view{reinterpret_cast<void *>(0x40), 0, 512};
  EXPECT_CALL(impl, allocate(512, 1)).WillOnce(Return(view));

  auto block = policy.addChunk(512, 1);

  EXPECT_EQ(policy.findChunkSize(block.handle), 512u);

  MockCpuResource::reset();
}

TEST(DirectChunkLocator, FindChunkSizeReturnsZeroForInvalidId) {
  Policy policy;
  MockCpuResource resource;
  Policy::Config cfg{};

  NiceMock<MockCpuResourceImpl> impl;
  MockCpuResource::set(&impl);
  policy.initialize(cfg, &resource);

  BufferHandle invalid{123456};
  EXPECT_EQ(policy.findChunkSize(invalid), 0u);

  MockCpuResource::reset();
}

TEST(DirectChunkLocator, IsAliveReflectsState) {
  Policy policy;
  MockCpuResource resource;
  Policy::Config cfg{};

  NiceMock<MockCpuResourceImpl> impl;
  MockCpuResource::set(&impl);
  policy.initialize(cfg, &resource);

  CpuView view{reinterpret_cast<void *>(0x70), 0, 64};
  EXPECT_CALL(impl, allocate(64, 1)).WillOnce(Return(view));
  EXPECT_CALL(impl, deallocate(view, 64, 1)).Times(1);

  auto block = policy.addChunk(64, 1);
  EXPECT_TRUE(policy.isAlive(block.handle));
  EXPECT_TRUE(policy.releaseChunk(block.handle));
  EXPECT_FALSE(policy.isAlive(block.handle));

  MockCpuResource::reset();
}

} // namespace

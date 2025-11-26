#include "orteaf/internal/runtime/allocator/policies/chunk_locator/direct_chunk_locator.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/backend/backend_traits.h"
#include "orteaf/internal/backend/cpu/cpu_buffer_view.h"
#include "tests/internal/runtime/allocator/testing/mock_resource.h"
#include "tests/internal/testing/error_assert.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace allocator = ::orteaf::internal::runtime::allocator;
namespace policies = ::orteaf::internal::runtime::allocator::policies;
using Backend = ::orteaf::internal::backend::Backend;
using BufferId = ::orteaf::internal::base::BufferId;
using CpuView = ::orteaf::internal::backend::cpu::CpuBufferView;
using Traits = ::orteaf::internal::backend::BackendTraits<Backend::Cpu>;
using Device = Traits::Device;
using Stream = Traits::Stream;
using ::orteaf::internal::runtime::allocator::testing::MockCpuResource;
using ::orteaf::internal::runtime::allocator::testing::MockCpuResourceImpl;

namespace {

using Policy = policies::DirectChunkLocatorPolicy<MockCpuResource, Backend::Cpu>;

TEST(DirectChunkLocator, ReleaseChunkCallsResourceWhenFree) {
    Policy policy;
    MockCpuResource resource;
    Device device = 42;
    Stream stream = reinterpret_cast<void*>(0x1234);
    Policy::Config cfg{};
    cfg.device = device;
    cfg.context = 0;
    cfg.stream = stream;

    NiceMock<MockCpuResourceImpl> impl;
    MockCpuResource::set(&impl);
    policy.initialize(cfg, &resource);

    CpuView view{reinterpret_cast<void*>(0x10), 0, 256};
    EXPECT_CALL(impl, allocate(256, 1, stream)).WillOnce(Return(view));
    EXPECT_CALL(impl, deallocate(view, 256, 1, stream)).Times(1);

    auto block = policy.addChunk(256, 1);
    BufferId id = block.id;

    EXPECT_TRUE(policy.releaseChunk(id));
    EXPECT_FALSE(policy.isAlive(id));

    MockCpuResource::reset();
}

TEST(DirectChunkLocator, InitializeFailsWithNullResource) {
    Policy policy;
    Policy::Config cfg{};
    cfg.device = 0;
    cfg.context = 0;
    cfg.stream = nullptr;

    orteaf::tests::ExpectError(::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
                               [&] { policy.initialize(cfg, nullptr); });
}

TEST(DirectChunkLocator, AddChunkBeforeInitializeThrows) {
    Policy policy;
    orteaf::tests::ExpectError(::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
                               [&] { policy.addChunk(64, 1); });
}

TEST(DirectChunkLocator, ReleaseChunkSkipsWhenInUse) {
    Policy policy;
    MockCpuResource resource;
    Device device = 1;
    Stream stream = nullptr;
    Policy::Config cfg{};
    cfg.device = device;
    cfg.context = 0;
    cfg.stream = stream;

    NiceMock<MockCpuResourceImpl> impl;
    MockCpuResource::set(&impl);
    policy.initialize(cfg, &resource);

    CpuView view{reinterpret_cast<void*>(0x20), 0, 128};
    EXPECT_CALL(impl, allocate(128, 1, stream)).WillOnce(Return(view));
    EXPECT_CALL(impl, deallocate(view, 128, 1, stream)).Times(1);

    auto block = policy.addChunk(128, 1);
    BufferId id = block.id;
    policy.incrementUsed(id);

    EXPECT_FALSE(policy.releaseChunk(id));

    policy.decrementUsed(id);
    EXPECT_TRUE(policy.releaseChunk(id));

    MockCpuResource::reset();
}

TEST(DirectChunkLocator, DoubleReleaseFails) {
    Policy policy;
    MockCpuResource resource;
    Policy::Config cfg{};
    cfg.device = 11;
    cfg.context = 0;
    cfg.stream = nullptr;

    NiceMock<MockCpuResourceImpl> impl;
    MockCpuResource::set(&impl);
    policy.initialize(cfg, &resource);

    CpuView view{reinterpret_cast<void*>(0x21), 0, 128};
    EXPECT_CALL(impl, allocate(128, 1, nullptr)).WillOnce(Return(view));
    EXPECT_CALL(impl, deallocate(view, 128, 1, nullptr)).Times(1);

    auto block = policy.addChunk(128, 1);
    BufferId id = block.id;
    EXPECT_TRUE(policy.releaseChunk(id));
    EXPECT_FALSE(policy.releaseChunk(id));
    EXPECT_FALSE(policy.isAlive(id));

    MockCpuResource::reset();
}

TEST(DirectChunkLocator, PendingBlocksPreventRelease) {
    Policy policy;
    MockCpuResource resource;
    Policy::Config cfg{};
    cfg.device = 2;
    cfg.context = 0;
    cfg.stream = nullptr;

    NiceMock<MockCpuResourceImpl> impl;
    MockCpuResource::set(&impl);
    policy.initialize(cfg, &resource);

    CpuView view{reinterpret_cast<void*>(0x30), 0, 64};
    EXPECT_CALL(impl, allocate(64, 1, nullptr)).WillOnce(Return(view));
    EXPECT_CALL(impl, deallocate(view, 64, 1, nullptr)).Times(1);

    auto block = policy.addChunk(64, 1);
    BufferId id = block.id;
    policy.incrementPending(id);

    EXPECT_FALSE(policy.releaseChunk(id));

    policy.decrementPending(id);
    EXPECT_TRUE(policy.releaseChunk(id));

    MockCpuResource::reset();
}

TEST(DirectChunkLocator, OperationsOnInvalidIdAreNoops) {
    Policy policy;
    MockCpuResource resource;
    Policy::Config cfg{};
    cfg.device = 5;
    cfg.context = 0;
    cfg.stream = nullptr;

    NiceMock<MockCpuResourceImpl> impl;
    MockCpuResource::set(&impl);
    policy.initialize(cfg, &resource);

    BufferId invalid{99999};
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
    cfg.device = 3;
    cfg.context = 0;
    cfg.stream = nullptr;

    NiceMock<MockCpuResourceImpl> impl;
    MockCpuResource::set(&impl);
    policy.initialize(cfg, &resource);

    CpuView view{reinterpret_cast<void*>(0x40), 0, 512};
    EXPECT_CALL(impl, allocate(512, 1, nullptr)).WillOnce(Return(view));

    auto block = policy.addChunk(512, 1);

    EXPECT_EQ(policy.findChunkSize(block.id), 512u);

    MockCpuResource::reset();
}

TEST(DirectChunkLocator, FindChunkSizeReturnsZeroForInvalidId) {
    Policy policy;
    MockCpuResource resource;
    Policy::Config cfg{};
    cfg.device = 6;
    cfg.context = 0;
    cfg.stream = nullptr;

    NiceMock<MockCpuResourceImpl> impl;
    MockCpuResource::set(&impl);
    policy.initialize(cfg, &resource);

    BufferId invalid{123456};
    EXPECT_EQ(policy.findChunkSize(invalid), 0u);

    MockCpuResource::reset();
}

TEST(DirectChunkLocator, IsAliveReflectsState) {
    Policy policy;
    MockCpuResource resource;
    Policy::Config cfg{};
    cfg.device = 7;
    cfg.context = 0;
    cfg.stream = nullptr;

    NiceMock<MockCpuResourceImpl> impl;
    MockCpuResource::set(&impl);
    policy.initialize(cfg, &resource);

    CpuView view{reinterpret_cast<void*>(0x70), 0, 64};
    EXPECT_CALL(impl, allocate(64, 1, nullptr)).WillOnce(Return(view));
    EXPECT_CALL(impl, deallocate(view, 64, 1, nullptr)).Times(1);

    auto block = policy.addChunk(64, 1);
    EXPECT_TRUE(policy.isAlive(block.id));
    EXPECT_TRUE(policy.releaseChunk(block.id));
    EXPECT_FALSE(policy.isAlive(block.id));

    MockCpuResource::reset();
}

}  // namespace

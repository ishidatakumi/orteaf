#include "orteaf/internal/runtime/allocator/policies/chunk_locator/direct_chunk_locator.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/backend/backend_traits.h"
#include "orteaf/internal/backend/cpu/cpu_buffer_view.h"
#include "tests/internal/runtime/allocator/testing/mock_resource.h"

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
    EXPECT_CALL(impl, allocate(256, 1, device, stream)).WillOnce(Return(view));
    EXPECT_CALL(impl, deallocate(view, 256, 1, device, stream)).Times(1);

    auto block = policy.addChunk(256, 1);
    BufferId id = block.id;

    EXPECT_TRUE(policy.releaseChunk(id));
    EXPECT_FALSE(policy.isAlive(id));

    MockCpuResource::reset();
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
    EXPECT_CALL(impl, allocate(128, 1, device, stream)).WillOnce(Return(view));
    EXPECT_CALL(impl, deallocate(view, 128, 1, device, stream)).Times(1);

    auto block = policy.addChunk(128, 1);
    BufferId id = block.id;
    policy.incrementUsed(id);

    EXPECT_FALSE(policy.releaseChunk(id));

    policy.decrementUsed(id);
    EXPECT_TRUE(policy.releaseChunk(id));

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
    EXPECT_CALL(impl, allocate(64, 1, 2, nullptr)).WillOnce(Return(view));
    EXPECT_CALL(impl, deallocate(view, 64, 1, 2, nullptr)).Times(1);

    auto block = policy.addChunk(64, 1);
    BufferId id = block.id;
    policy.incrementPending(id);

    EXPECT_FALSE(policy.releaseChunk(id));

    policy.decrementPending(id);
    EXPECT_TRUE(policy.releaseChunk(id));

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
    EXPECT_CALL(impl, allocate(512, 1, 3, nullptr)).WillOnce(Return(view));

    auto block = policy.addChunk(512, 1);

    EXPECT_EQ(policy.findChunkSize(block.id), 512u);

    MockCpuResource::reset();
}

}  // namespace

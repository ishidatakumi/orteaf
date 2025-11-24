#include "orteaf/internal/runtime/allocator/policies/chunk_locator/hierarchical_chunk_locator.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/backend/backend_traits.h"
#include "tests/internal/runtime/allocator/testing/mock_resource.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::ReturnArg;

namespace allocator = ::orteaf::internal::runtime::allocator;
namespace policies = ::orteaf::internal::runtime::allocator::policies;
using Backend = ::orteaf::internal::backend::Backend;
using Traits = ::orteaf::internal::backend::BackendTraits<Backend::Cpu>;
using Device = Traits::Device;
using Context = Traits::Context;
using Stream = Traits::Stream;
using BufferView = Traits::BufferView;
using ::orteaf::internal::runtime::allocator::testing::MockCpuResource;
using ::orteaf::internal::runtime::allocator::testing::MockCpuResourceImpl;

namespace {

using Policy = policies::HierarchicalChunkLocator<MockCpuResource, Backend::Cpu>;

TEST(HierarchicalChunkLocator, ReusesSpanWithoutExtraReserve) {
    Policy policy;
    Device device = 7;
    Context context = 0;
    Stream stream = nullptr;
    typename Policy::Config cfg{device, context, stream, {256, 128}};

    MockCpuResource resource;
    policy.initialize(cfg, &resource);

    NiceMock<MockCpuResourceImpl> impl;
    MockCpuResource::set(&impl);

    // ルート拡張は最初の一回だけ
    void* base = reinterpret_cast<void*>(0x1000);
    EXPECT_CALL(impl, reserve(256, _, device, stream))
        .Times(2)
        .WillOnce([&](std::size_t, std::size_t, Device, Stream) {
            return BufferView{base, 0, 256};
        });

    EXPECT_CALL(impl, map(_, device, context, stream)).Times(4).WillRepeatedly(ReturnArg<0>());
    EXPECT_CALL(impl, unmap(_, _, _, device, context, stream)).Times(4);

    auto b1 = policy.allocate(128);
    auto b2 = policy.allocate(128);
    policy.deallocate(b1.id, 128, 0);
    policy.deallocate(b2.id, 128, 0);

    // ここからは再利用で reserve が増えないことを確認
    auto b3 = policy.allocate(128);
    auto b4 = policy.allocate(128);
    policy.deallocate(b3.id, 128, 0);
    policy.deallocate(b4.id, 128, 0);

    MockCpuResource::reset();
}

}  // namespace


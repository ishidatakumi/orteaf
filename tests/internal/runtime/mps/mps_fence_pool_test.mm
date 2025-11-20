#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>

#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/runtime/manager/mps/mps_fence_pool.h"
#include "tests/internal/runtime/mps/testing/backend_ops_provider.h"
#include "tests/internal/runtime/mps/testing/manager_test_fixture.h"
#include "tests/internal/testing/error_assert.h"

namespace backend = orteaf::internal::backend;
namespace diag_error = orteaf::internal::diagnostics::error;
namespace mps_rt = orteaf::internal::runtime::mps;
namespace testing_mps = orteaf::tests::runtime::mps::testing;

using orteaf::tests::ExpectError;

namespace {

backend::mps::MPSFence_t makeFence(std::uintptr_t value) {
    return reinterpret_cast<backend::mps::MPSFence_t>(value);
}

template <class Provider>
class MpsFencePoolTypedTest
    : public testing_mps::RuntimeManagerFixture<Provider, mps_rt::MpsFencePool> {
protected:
    using Base = testing_mps::RuntimeManagerFixture<Provider, mps_rt::MpsFencePool>;
    mps_rt::MpsFencePool& pool() { return Base::manager(); }
    auto& adapter() { return Base::adapter(); }
};

#if ORTEAF_ENABLE_MPS
using ProviderTypes = ::testing::Types<
    testing_mps::MockBackendOpsProvider,
    testing_mps::RealBackendOpsProvider>;
#else
using ProviderTypes = ::testing::Types<
    testing_mps::MockBackendOpsProvider>;
#endif

}  // namespace

TYPED_TEST_SUITE(MpsFencePoolTypedTest, ProviderTypes);

TYPED_TEST(MpsFencePoolTypedTest, GrowthChunkSizeCanBeAdjusted) {
    auto& pool = this->pool();
    EXPECT_EQ(pool.growthChunkSize(), 1u);
    pool.setGrowthChunkSize(4);
    EXPECT_EQ(pool.growthChunkSize(), 4u);
}

TYPED_TEST(MpsFencePoolTypedTest, GrowthChunkSizeRejectsZero) {
    auto& pool = this->pool();
    ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] { pool.setGrowthChunkSize(0); });
}

TYPED_TEST(MpsFencePoolTypedTest, InitializeRejectsNullDevice) {
    auto& pool = this->pool();
    ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] { pool.initialize(nullptr, this->getOps(), 1); });
}

TYPED_TEST(MpsFencePoolTypedTest, OperationsBeforeInitializationThrow) {
    auto& pool = this->pool();
    ExpectError(diag_error::OrteafErrc::InvalidState, [&] { (void)pool.acquireFence(); });
}

TYPED_TEST(MpsFencePoolTypedTest, InitializePreallocatesFences) {
    auto& pool = this->pool();
    const auto device = this->adapter().device();
    if constexpr (TypeParam::is_mock) {
        this->adapter().expectCreateFences({makeFence(0x100), makeFence(0x101)});
    }
    pool.initialize(device, this->getOps(), 2);
    EXPECT_EQ(pool.availableCount(), 2u);
    auto first = pool.acquireFence();
    auto second = pool.acquireFence();
    EXPECT_NE(first.get(), nullptr);
    EXPECT_NE(second.get(), nullptr);
    EXPECT_EQ(pool.availableCount(), 0u);
    first.release();
    second.release();
    if constexpr (TypeParam::is_mock) {
        this->adapter().expectDestroyFences({makeFence(0x100), makeFence(0x101)});
    }
    pool.shutdown();
}

TYPED_TEST(MpsFencePoolTypedTest, GrowChunkAllocatesInBlocks) {
    auto& pool = this->pool();
    pool.setGrowthChunkSize(2);
    const auto device = this->adapter().device();
    if constexpr (TypeParam::is_mock) {
        this->adapter().expectCreateFences({});
    }
    pool.initialize(device, this->getOps(), 0);
    if constexpr (TypeParam::is_mock) {
        this->adapter().expectCreateFences({makeFence(0x200), makeFence(0x201)});
    }
    auto first = pool.acquireFence();
    EXPECT_NE(first.get(), nullptr);
    EXPECT_EQ(pool.availableCount(), 1u);
    auto second = pool.acquireFence();
    EXPECT_EQ(pool.availableCount(), 0u);
    if constexpr (TypeParam::is_mock) {
        this->adapter().expectCreateFences({makeFence(0x202), makeFence(0x203)});
    }
    auto third = pool.acquireFence();
    EXPECT_NE(third.get(), nullptr);
    EXPECT_EQ(pool.availableCount(), 1u);
    first.release();
    second.release();
    third.release();
    if constexpr (TypeParam::is_mock) {
        this->adapter().expectDestroyFences({makeFence(0x200), makeFence(0x201), makeFence(0x202), makeFence(0x203)});
    }
    pool.shutdown();
}

TYPED_TEST(MpsFencePoolTypedTest, ShutdownRejectsInUseFences) {
    auto& pool = this->pool();
    const auto device = this->adapter().device();
    if constexpr (TypeParam::is_mock) {
        this->adapter().expectCreateFences({makeFence(0x350)});
    }
    pool.initialize(device, this->getOps(), 1);
    auto handle = pool.acquireFence();
    ExpectError(diag_error::OrteafErrc::InvalidState, [&] { pool.shutdown(); });
    handle.release();
    if constexpr (TypeParam::is_mock) {
        this->adapter().expectDestroyFences({makeFence(0x350)});
    }
    pool.shutdown();
}

TYPED_TEST(MpsFencePoolTypedTest, DebugStateReflectsCounts) {
    auto& pool = this->pool();
    pool.setGrowthChunkSize(5);
    const auto device = this->adapter().device();
    if constexpr (TypeParam::is_mock) {
        this->adapter().expectCreateFences({makeFence(0x360), makeFence(0x361)});
    }
    pool.initialize(device, this->getOps(), 2);
    auto handle = pool.acquireFence();
    const auto snapshot = pool.debugState();
    EXPECT_EQ(snapshot.growth_chunk_size, 5u);
    EXPECT_EQ(snapshot.available_count, 1u);
    EXPECT_EQ(snapshot.in_use_count, 1u);
    EXPECT_EQ(snapshot.total_created, 2u);
    handle.release();
    if constexpr (TypeParam::is_mock) {
        this->adapter().expectDestroyFences({makeFence(0x360), makeFence(0x361)});
    }
    pool.shutdown();
}

TYPED_TEST(MpsFencePoolTypedTest, MovedFromHandleIsInactive) {
    auto& pool = this->pool();
    const auto device = this->adapter().device();
    if constexpr (TypeParam::is_mock) {
        this->adapter().expectCreateFences({makeFence(0x410)});
    }
    pool.initialize(device, this->getOps(), 1);
    auto h1 = pool.acquireFence();
    auto h2 = std::move(h1);
    ExpectError(diag_error::OrteafErrc::InvalidState, [&] { (void)h1.get(); });
    EXPECT_NE(h2.get(), nullptr);
    h2.release();
    if constexpr (TypeParam::is_mock) {
        this->adapter().expectDestroyFences({makeFence(0x410)});
    }
    pool.shutdown();
}

TYPED_TEST(MpsFencePoolTypedTest, DestructionReturnsHandleToPool) {
    auto& pool = this->pool();
    const auto device = this->adapter().device();
    if constexpr (TypeParam::is_mock) {
        this->adapter().expectCreateFences({makeFence(0x420)});
    }
    pool.initialize(device, this->getOps(), 1);
    {
        auto h = pool.acquireFence();
        EXPECT_EQ(pool.availableCount(), 0u);
        EXPECT_EQ(pool.inUseCount(), 1u);
        EXPECT_NE(h.get(), nullptr);
    }
    EXPECT_EQ(pool.availableCount(), 1u);
    EXPECT_EQ(pool.inUseCount(), 0u);
    if constexpr (TypeParam::is_mock) {
        this->adapter().expectDestroyFences({makeFence(0x420)});
    }
    pool.shutdown();
}

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <utility>

#include <orteaf/internal/diagnostics/error/error.h>
#include <orteaf/internal/runtime/mps/manager/mps_event_pool.h>
#include <tests/internal/runtime/mps/manager/testing/backend_ops_provider.h>
#include <tests/internal/runtime/mps/manager/testing/manager_test_fixture.h>
#include <tests/internal/testing/error_assert.h>

namespace backend = orteaf::internal::backend;
namespace diag_error = orteaf::internal::diagnostics::error;
namespace mps_rt = orteaf::internal::runtime::mps::manager;
namespace mps_wrapper = orteaf::internal::runtime::mps::platform::wrapper;
namespace testing_mps = orteaf::tests::runtime::mps::testing;

using orteaf::tests::ExpectError;

namespace {

mps_wrapper::MPSEvent_t makeEvent(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MPSEvent_t>(value);
}

template <class Provider>
class MpsEventPoolTypedTest
    : public testing_mps::RuntimeManagerFixture<Provider,
                                                mps_rt::MpsEventPool> {
protected:
  using Base =
      testing_mps::RuntimeManagerFixture<Provider, mps_rt::MpsEventPool>;
  mps_rt::MpsEventPool &pool() { return Base::manager(); }
  auto &adapter() { return Base::adapter(); }
};

#if ORTEAF_ENABLE_MPS
using ProviderTypes = ::testing::Types<testing_mps::MockBackendOpsProvider,
                                       testing_mps::RealBackendOpsProvider>;
#else
using ProviderTypes = ::testing::Types<testing_mps::MockBackendOpsProvider>;
#endif

} // namespace

TYPED_TEST_SUITE(MpsEventPoolTypedTest, ProviderTypes);

TYPED_TEST(MpsEventPoolTypedTest, GrowthChunkSizeCanBeAdjusted) {
  auto &pool = this->pool();
  EXPECT_EQ(pool.growthChunkSize(), 1u);
  pool.setGrowthChunkSize(4);
  EXPECT_EQ(pool.growthChunkSize(), 4u);
}

TYPED_TEST(MpsEventPoolTypedTest, GrowthChunkSizeRejectsZero) {
  auto &pool = this->pool();
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { pool.setGrowthChunkSize(0); });
}

TYPED_TEST(MpsEventPoolTypedTest, InitializeRejectsNullDevice) {
  auto &pool = this->pool();
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { pool.initialize(nullptr, this->getOps(), 1); });
}

TYPED_TEST(MpsEventPoolTypedTest, OperationsBeforeInitializationThrow) {
  auto &pool = this->pool();
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)pool.acquireEvent(); });
}

TYPED_TEST(MpsEventPoolTypedTest, InitializePreallocatesEvents) {
  auto &pool = this->pool();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x100), makeEvent(0x101)});
  }
  pool.initialize(device, this->getOps(), 2);
  EXPECT_EQ(pool.availableCount(), 2u);
  auto first = pool.acquireEvent();
  auto second = pool.acquireEvent();
  EXPECT_TRUE(first);
  EXPECT_TRUE(second);
  EXPECT_EQ(pool.availableCount(), 0u);
  first.release();
  second.release();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0x100), makeEvent(0x101)});
  }
  pool.shutdown();
}

TYPED_TEST(MpsEventPoolTypedTest, GrowChunkAllocatesInBlocks) {
  auto &pool = this->pool();
  pool.setGrowthChunkSize(2);
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({});
  }
  pool.initialize(device, this->getOps(), 0);
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x200), makeEvent(0x201)});
  }
  auto first = pool.acquireEvent();
  EXPECT_TRUE(first);
  EXPECT_EQ(pool.availableCount(), 1u);
  auto second = pool.acquireEvent();
  EXPECT_EQ(pool.availableCount(), 0u);
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x202), makeEvent(0x203)});
  }
  auto third = pool.acquireEvent();
  EXPECT_TRUE(third);
  EXPECT_EQ(pool.availableCount(), 1u);
  first.release();
  second.release();
  third.release();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0x200), makeEvent(0x201),
                                         makeEvent(0x202), makeEvent(0x203)});
  }
  pool.shutdown();
}

TYPED_TEST(MpsEventPoolTypedTest, ShutdownRejectsInUseEvents) {
  auto &pool = this->pool();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x350)});
  }
  pool.initialize(device, this->getOps(), 1);
  auto handle = pool.acquireEvent();
  ExpectError(diag_error::OrteafErrc::InvalidState, [&] { pool.shutdown(); });
  handle.release();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0x350)});
  }
  pool.shutdown();
}

TYPED_TEST(MpsEventPoolTypedTest, DebugStateReflectsCounts) {
  auto &pool = this->pool();
  pool.setGrowthChunkSize(5);
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x360), makeEvent(0x361)});
  }
  pool.initialize(device, this->getOps(), 2);
  auto handle = pool.acquireEvent();
  const auto snapshot = pool.debugState();
  EXPECT_EQ(snapshot.growth_chunk_size, 5u);
  EXPECT_EQ(snapshot.available_count, 1u);
  EXPECT_EQ(snapshot.in_use_count, 1u);
  EXPECT_EQ(snapshot.total_created, 2u);
  handle.release();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0x360), makeEvent(0x361)});
  }
  pool.shutdown();
}

TYPED_TEST(MpsEventPoolTypedTest, MovedFromHandleIsInactive) {
  auto &pool = this->pool();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x410)});
  }
  pool.initialize(device, this->getOps(), 1);
  auto h1 = pool.acquireEvent();
  auto h2 = std::move(h1);
  EXPECT_FALSE(h1);
  EXPECT_TRUE(h2);
  h2.release();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0x410)});
  }
  pool.shutdown();
}

TYPED_TEST(MpsEventPoolTypedTest, DestructionReturnsHandleToPool) {
  auto &pool = this->pool();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x420)});
  }
  pool.initialize(device, this->getOps(), 1);
  {
    auto h = pool.acquireEvent();
    EXPECT_EQ(pool.availableCount(), 0u);
    EXPECT_EQ(pool.inUseCount(), 1u);
    EXPECT_TRUE(h);
  }
  EXPECT_EQ(pool.availableCount(), 1u);
  EXPECT_EQ(pool.inUseCount(), 0u);
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0x420)});
  }
  pool.shutdown();
}

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>

#include <orteaf/internal/diagnostics/error/error.h>
#include <orteaf/internal/execution/mps/manager/mps_fence_manager.h>
#include <tests/internal/execution/mps/manager/testing/execution_ops_provider.h>
#include <tests/internal/execution/mps/manager/testing/manager_test_fixture.h>
#include <tests/internal/testing/error_assert.h>

namespace base = orteaf::internal::base;
namespace diag_error = orteaf::internal::diagnostics::error;
namespace mps_rt = orteaf::internal::execution::mps::manager;
namespace mps_wrapper = orteaf::internal::execution::mps::platform::wrapper;
namespace testing_mps = orteaf::tests::execution::mps::testing;

using orteaf::tests::ExpectError;

namespace {

mps_wrapper::MpsFence_t makeFence(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MpsFence_t>(value);
}

mps_rt::MpsFenceManager::Config makeConfig(
    mps_wrapper::MpsDevice_t device, mps_rt::MpsFenceManager::SlowOps *ops,
    std::size_t payload_capacity, std::size_t control_block_capacity,
    std::size_t payload_block_size, std::size_t control_block_block_size,
    std::size_t payload_growth_chunk_size,
    std::size_t control_block_growth_chunk_size) {
  mps_rt::MpsFenceManager::Config config{};
  config.device = device;
  config.ops = ops;
  config.pool.payload_capacity = payload_capacity;
  config.pool.control_block_capacity = control_block_capacity;
  config.pool.payload_block_size = payload_block_size;
  config.pool.control_block_block_size = control_block_block_size;
  config.pool.payload_growth_chunk_size = payload_growth_chunk_size;
  config.pool.control_block_growth_chunk_size = control_block_growth_chunk_size;
  return config;
}

template <class Provider>
class MpsFenceManagerTypedTest
    : public testing_mps::RuntimeManagerFixture<Provider,
                                                mps_rt::MpsFenceManager> {
protected:
  using Base =
      testing_mps::RuntimeManagerFixture<Provider, mps_rt::MpsFenceManager>;
  mps_rt::MpsFenceManager &manager() { return Base::manager(); }
  auto &adapter() { return Base::adapter(); }
};

#if ORTEAF_ENABLE_MPS
using ProviderTypes = ::testing::Types<testing_mps::MockExecutionOpsProvider,
                                       testing_mps::RealExecutionOpsProvider>;
#else
using ProviderTypes = ::testing::Types<testing_mps::MockExecutionOpsProvider>;
#endif

} // namespace

TYPED_TEST_SUITE(MpsFenceManagerTypedTest, ProviderTypes);

// =============================================================================
// Initialization Tests
// =============================================================================

TYPED_TEST(MpsFenceManagerTypedTest, InitializeRejectsNullDevice) {
  auto &manager = this->manager();

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { manager.configure(makeConfig(nullptr, this->getOps(), 1, 1, 1, 1, 1, 1)); });
}

TYPED_TEST(MpsFenceManagerTypedTest, InitializeRejectsNullOps) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { manager.configure(makeConfig(device, nullptr, 1, 1, 1, 1, 1, 1)); });
}

TYPED_TEST(MpsFenceManagerTypedTest, OperationsBeforeInitializationThrow) {
  auto &manager = this->manager();

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.acquire(); });
}

TYPED_TEST(MpsFenceManagerTypedTest, InitializeEagerlyCreatesFences) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateFences({makeFence(0x100), makeFence(0x101)},
                                       ::testing::Eq(device));
  }
  manager.configure(makeConfig(device, this->getOps(), 2, 2, 2, 2, 1, 1));

  // Act: First acquire uses existing fence
  auto first = manager.acquire();
  EXPECT_TRUE(first);

  // Act: Second acquire uses another pre-created fence
  auto second = manager.acquire();
  EXPECT_TRUE(second);

  // Cleanup
  first.release();
  second.release();

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyFences({makeFence(0x100), makeFence(0x101)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsFenceManagerTypedTest, InitializeWithZeroCapacitySucceeds) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Arrange
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateFences({}, ::testing::Eq(device));
  }
  manager.configure(makeConfig(device, this->getOps(), 0, 0, 1, 1, 1, 1));

  // Act: Should grow on demand
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateFences({makeFence(0x200)},
                                       ::testing::Eq(device));
  }
  auto fence = manager.acquire();

  // Assert
  EXPECT_TRUE(fence);

  // Cleanup
  fence.release();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyFences({makeFence(0x200)});
  }
  manager.shutdown();
}

// =============================================================================
// Acquire Tests
// =============================================================================

TYPED_TEST(MpsFenceManagerTypedTest, AcquireReturnsValidLease) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateFences({makeFence(0x300)},
                                       ::testing::Eq(device));
  }
  manager.configure(makeConfig(device, this->getOps(), 1, 1, 1, 1, 1, 1));

  // Arrange
  // Act
  auto lease = manager.acquire();

  // Assert
  EXPECT_TRUE(lease);
  ASSERT_NE(lease.payloadPtr(), nullptr);
  EXPECT_TRUE(lease.handle().isValid());
  EXPECT_TRUE(lease.payloadPtr()->hasFence());
  EXPECT_FALSE(lease.payloadPtr()->hasCommandBuffer());
  EXPECT_TRUE(lease.payloadPtr()->isCompleted());

  // Cleanup
  lease.release();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyFences({makeFence(0x300)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsFenceManagerTypedTest, AcquireWeakByHandleReturnsLease) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateFences({makeFence(0x305)},
                                       ::testing::Eq(device));
  }
  manager.configure(makeConfig(device, this->getOps(), 1, 1, 1, 1, 1, 1));

  auto strong_lease = manager.acquire();
  ASSERT_TRUE(strong_lease);
  EXPECT_EQ(strong_lease.strongCount(), 1u);

  auto weak_lease = manager.acquireWeak(strong_lease.payloadHandle());
  EXPECT_TRUE(weak_lease);
  EXPECT_EQ(weak_lease.weakCount(), 1u);

  auto locked = weak_lease.lock();
  EXPECT_TRUE(locked);
  EXPECT_EQ(locked.strongCount(), 2u);
  locked.release();
  EXPECT_EQ(strong_lease.strongCount(), 1u);

  weak_lease.release();
  strong_lease.release();

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyFences({makeFence(0x305)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsFenceManagerTypedTest, AcquireWeakRejectsInvalidHandle) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateFences({makeFence(0x306)},
                                       ::testing::Eq(device));
  }
  manager.configure(makeConfig(device, this->getOps(), 1, 1, 1, 1, 1, 1));

  using Handle = mps_rt::MpsFenceManager::FenceHandle;
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { (void)manager.acquireWeak(Handle::invalid()); });

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyFences({makeFence(0x306)});
  }
  manager.shutdown();
}

// =============================================================================
// Release Tests
// =============================================================================

TYPED_TEST(MpsFenceManagerTypedTest, FenceRecyclingReusesSlots) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateFences({makeFence(0x800)},
                                       ::testing::Eq(device));
  }
  manager.configure(makeConfig(device, this->getOps(), 1, 1, 1, 1, 1, 1));

  // Arrange
  // Act
  auto first = manager.acquire();
  const auto first_index = first.handle().index;
  first.release();

  auto second = manager.acquire();

  // Assert: Same slot and fence reused
  EXPECT_EQ(second.handle().index, first_index);

  // Cleanup
  second.release();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyFences({makeFence(0x800)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsFenceManagerTypedTest, MovedFromLeaseIsInactive) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateFences({makeFence(0x900)},
                                       ::testing::Eq(device));
  }
  manager.configure(makeConfig(device, this->getOps(), 1, 1, 1, 1, 1, 1));

  // Arrange
  // Act
  auto lease1 = manager.acquire();
  auto lease2 = std::move(lease1);

  // Assert
  EXPECT_FALSE(lease1);
  EXPECT_TRUE(lease2);

  // Cleanup
  lease2.release();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyFences({makeFence(0x900)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsFenceManagerTypedTest, DestructionReturnsFenceToPool) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateFences({makeFence(0xA00)},
                                       ::testing::Eq(device));
  }
  manager.configure(makeConfig(device, this->getOps(), 1, 1, 1, 1, 1, 1));

  // Arrange
  std::uint32_t index;
  {
    auto lease = manager.acquire();
    index = lease.handle().index;
    EXPECT_TRUE(lease);
    // lease destructor releases
  }

  // Assert: Can reuse slot
  auto new_lease = manager.acquire();
  EXPECT_EQ(new_lease.handle().index, index);

  // Cleanup
  new_lease.release();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyFences({makeFence(0xA00)});
  }
  manager.shutdown();
}

// =============================================================================
// Shutdown Tests
// =============================================================================

TYPED_TEST(MpsFenceManagerTypedTest, ShutdownReleasesInitializedFences) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateFences(
        {makeFence(0xB00), makeFence(0xB01), makeFence(0xB02)},
        ::testing::Eq(device));
  }
  manager.configure(makeConfig(device, this->getOps(), 3, 3, 3, 3, 1, 1));

  // Arrange: Acquire two fences
  auto lease1 = manager.acquire();

  auto lease2 = manager.acquire();

  lease1.release();
  lease2.release();

  // Act & Assert: All created fences destroyed
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyFences(
        {makeFence(0xB00), makeFence(0xB01), makeFence(0xB02)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsFenceManagerTypedTest, ShutdownWithoutInitializeIsNoOp) {
  auto &manager = this->manager();

  // Act & Assert
  EXPECT_NO_THROW(manager.shutdown());
  EXPECT_NO_THROW(manager.shutdown());
}

TYPED_TEST(MpsFenceManagerTypedTest, MultipleShutdownsAreIdempotent) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateFences({makeFence(0xC00)},
                                       ::testing::Eq(device));
  }
  manager.configure(makeConfig(device, this->getOps(), 1, 1, 1, 1, 1, 1));

  // Act & Assert
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyFences({makeFence(0xC00)});
  }
  manager.shutdown();
  EXPECT_NO_THROW(manager.shutdown());
  EXPECT_NO_THROW(manager.shutdown());
}

TYPED_TEST(MpsFenceManagerTypedTest, ReinitializeResetsPreviousState) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Arrange: First initialization
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateFences({makeFence(0xD00)},
                                       ::testing::Eq(device));
  }
  manager.configure(makeConfig(device, this->getOps(), 1, 1, 1, 1, 1, 1));

  auto first_lease = manager.acquire();
  first_lease.release();

  // Act: Reinitialize destroys previous fences
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyFences({makeFence(0xD00)});
  }
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateFences({makeFence(0xD10), makeFence(0xD11)},
                                       ::testing::Eq(device));
  }
  manager.configure(makeConfig(device, this->getOps(), 2, 2, 2, 2, 1, 1));

  // Assert: New fences available
  auto new_lease = manager.acquire();
  EXPECT_TRUE(new_lease);

  // Cleanup
  new_lease.release();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyFences({makeFence(0xD10), makeFence(0xD11)});
  }
  manager.shutdown();
}

// =============================================================================
// Debug State Tests
// =============================================================================

#if ORTEAF_ENABLE_TEST
TYPED_TEST(MpsFenceManagerTypedTest, DebugStateReflectsFenceState) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateFences({makeFence(0xE00)},
                                       ::testing::Eq(device));
  }
  manager.configure(makeConfig(device, this->getOps(), 1, 1, 1, 1, 1, 1));

  // Arrange
  // Act
  auto lease = manager.acquire();

  // Assert: Verify manager is initialized and fence is alive
  EXPECT_TRUE(manager.isConfiguredForTest());
  EXPECT_GT(manager.controlBlockPoolCapacityForTest(), 0u);

  // Cleanup
  lease.release();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyFences({makeFence(0xE00)});
  }
  manager.shutdown();
}

// =============================================================================
// Lease Copy/Move Tests with Count Verification
// =============================================================================

TYPED_TEST(MpsFenceManagerTypedTest, LeaseCopyIncrementsCount) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateFences({makeFence(0xF00)},
                                       ::testing::Eq(device));
  }
  manager.configure(makeConfig(device, this->getOps(), 1, 1, 1, 1, 1, 1));

  // Arrange
  // Act
  auto lease1 = manager.acquire();
  EXPECT_EQ(lease1.strongCount(), 1u);

  auto lease2 = lease1; // Copy

  // Assert: Both valid, count incremented
  EXPECT_TRUE(lease1);
  EXPECT_TRUE(lease2);
  EXPECT_EQ(lease1.strongCount(), 2u);
  EXPECT_EQ(lease2.strongCount(), 2u);

  // Cleanup
  lease1.release();
  EXPECT_EQ(lease2.strongCount(), 1u);
  lease2.release();

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyFences({makeFence(0xF00)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsFenceManagerTypedTest, LeaseCopyAssignmentIncrementsCount) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateFences({makeFence(0xF10)},
                                       ::testing::Eq(device));
  }
  manager.configure(makeConfig(device, this->getOps(), 1, 1, 1, 1, 1, 1));

  // Arrange
  auto lease1 = manager.acquire();
  decltype(lease1) lease2;

  // Act
  lease2 = lease1; // Copy assignment

  // Assert
  EXPECT_EQ(lease1.strongCount(), 2u);
  EXPECT_EQ(lease2.strongCount(), 2u);

  // Cleanup
  lease1.release();
  lease2.release();

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyFences({makeFence(0xF10)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsFenceManagerTypedTest, LeaseMoveDoesNotChangeCount) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateFences({makeFence(0xF20)},
                                       ::testing::Eq(device));
  }
  manager.configure(makeConfig(device, this->getOps(), 1, 1, 1, 1, 1, 1));

  // Arrange
  auto lease1 = manager.acquire();
  EXPECT_EQ(lease1.strongCount(), 1u);

  // Act
  auto lease2 = std::move(lease1); // Move

  // Assert: Source invalid, target valid, count unchanged
  EXPECT_FALSE(lease1);
  EXPECT_TRUE(lease2);
  EXPECT_EQ(lease2.strongCount(), 1u);

  // Cleanup
  lease2.release();

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyFences({makeFence(0xF20)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsFenceManagerTypedTest, LeaseMoveAssignmentDoesNotChangeCount) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateFences({makeFence(0xF30)},
                                       ::testing::Eq(device));
  }
  manager.configure(makeConfig(device, this->getOps(), 1, 1, 1, 1, 1, 1));

  // Arrange
  auto lease1 = manager.acquire();
  decltype(lease1) lease2;

  // Act
  lease2 = std::move(lease1); // Move assignment

  // Assert
  EXPECT_FALSE(lease1);
  EXPECT_TRUE(lease2);
  EXPECT_EQ(lease2.strongCount(), 1u);

  // Cleanup
  lease2.release();

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyFences({makeFence(0xF30)});
  }
  manager.shutdown();
}

#endif

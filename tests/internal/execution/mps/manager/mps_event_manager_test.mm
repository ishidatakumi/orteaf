#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>

#include <orteaf/internal/diagnostics/error/error.h>
#include <orteaf/internal/execution/mps/manager/mps_event_manager.h>
#include <tests/internal/execution/mps/manager/testing/backend_ops_provider.h>
#include <tests/internal/execution/mps/manager/testing/manager_test_fixture.h>
#include <tests/internal/testing/error_assert.h>

namespace base = orteaf::internal::base;
namespace diag_error = orteaf::internal::diagnostics::error;
namespace mps_rt = orteaf::internal::execution::mps::manager;
namespace mps_wrapper = orteaf::internal::execution::mps::platform::wrapper;
namespace testing_mps = orteaf::tests::execution::mps::testing;

using orteaf::tests::ExpectError;

namespace {

mps_wrapper::MpsEvent_t makeEvent(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MpsEvent_t>(value);
}

template <class Provider>
class MpsEventManagerTypedTest
    : public testing_mps::RuntimeManagerFixture<Provider,
                                                mps_rt::MpsEventManager> {
protected:
  using Base =
      testing_mps::RuntimeManagerFixture<Provider, mps_rt::MpsEventManager>;
  mps_rt::MpsEventManager &manager() { return Base::manager(); }
  auto &adapter() { return Base::adapter(); }
};

#if ORTEAF_ENABLE_MPS
using ProviderTypes = ::testing::Types<testing_mps::MockBackendOpsProvider,
                                       testing_mps::RealBackendOpsProvider>;
#else
using ProviderTypes = ::testing::Types<testing_mps::MockBackendOpsProvider>;
#endif

} // namespace

TYPED_TEST_SUITE(MpsEventManagerTypedTest, ProviderTypes);

// =============================================================================
// Initialization Tests
// =============================================================================

TYPED_TEST(MpsEventManagerTypedTest, InitializeRejectsNullDevice) {
  auto &manager = this->manager();

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { manager.configure(mps_rt::MpsEventManager::Config{nullptr, this->getOps(), 1, 1, 1, 1, 1, 1}); });
}

TYPED_TEST(MpsEventManagerTypedTest, InitializeRejectsNullOps) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { manager.configure(mps_rt::MpsEventManager::Config{device, nullptr, 1, 1, 1, 1, 1, 1}); });
}

TYPED_TEST(MpsEventManagerTypedTest, InitializeRejectsExcessiveCapacity) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  const std::size_t excessive =
      static_cast<std::size_t>(base::EventHandle::invalid_index()) + 1;

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { manager.configure(mps_rt::MpsEventManager::Config{device, this->getOps(), excessive, excessive, excessive, excessive, 1, 1}); });
}

TYPED_TEST(MpsEventManagerTypedTest, OperationsBeforeInitializationThrow) {
  auto &manager = this->manager();

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.acquire(); });
}

TYPED_TEST(MpsEventManagerTypedTest, InitializeEagerlyCreatesEvents) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x100), makeEvent(0x101)},
                                       ::testing::Eq(device));
  }
  manager.configure(mps_rt::MpsEventManager::Config{device, this->getOps(), 2, 2, 2, 2, 1, 1});

  // Act: First acquire uses existing event
  auto first = manager.acquire();
  EXPECT_TRUE(first);

  // Act: Second acquire uses another pre-created event
  auto second = manager.acquire();
  EXPECT_TRUE(second);

  // Cleanup
  manager.release(first);
  manager.release(second);

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0x100), makeEvent(0x101)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsEventManagerTypedTest, InitializeWithZeroCapacitySucceeds) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Arrange
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({}, ::testing::Eq(device));
  }
  manager.configure(mps_rt::MpsEventManager::Config{device, this->getOps(), 0, 0, 1, 1, 1, 1});

  // Act: Should grow on demand
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x200)},
                                       ::testing::Eq(device));
  }
  auto event = manager.acquire();

  // Assert
  EXPECT_TRUE(event);

  // Cleanup
  manager.release(event);
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0x200)});
  }
  manager.shutdown();
}

// =============================================================================
// Acquire Tests
// =============================================================================

TYPED_TEST(MpsEventManagerTypedTest, AcquireReturnsValidLease) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x300)},
                                       ::testing::Eq(device));
  }
  manager.configure(mps_rt::MpsEventManager::Config{device, this->getOps(), 1, 1, 1, 1, 1, 1});

  // Arrange
  // Act
  auto lease = manager.acquire();

  // Assert
  EXPECT_TRUE(lease);
  EXPECT_NE(lease.payloadPtr(), nullptr);
  EXPECT_TRUE(lease.payloadHandle().isValid());

  // Cleanup
  manager.release(lease);
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0x300)});
  }
  manager.shutdown();
}

// =============================================================================
// Release Tests
// =============================================================================

TYPED_TEST(MpsEventManagerTypedTest, ReleaseDecrementsRefCount) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x700), makeEvent(0x701)},
                                       ::testing::Eq(device));
  }
  manager.configure(mps_rt::MpsEventManager::Config{device, this->getOps(), 2, 2, 2, 2, 1, 1});

  // Arrange: Two acquires create two events
  auto lease1 = manager.acquire();
  auto lease2 = manager.acquire();

  // Act: Release one, another remains
  lease1.release();

  // Assert: Lease2 still valid
  EXPECT_TRUE(lease2);

  // Cleanup
  lease2.release();

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0x700), makeEvent(0x701)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsEventManagerTypedTest, EventRecyclingReusesSlots) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x800)},
                                       ::testing::Eq(device));
  }
  manager.configure(mps_rt::MpsEventManager::Config{device, this->getOps(), 1, 1, 1, 1, 1, 1});

  // Arrange
  // Act
  auto first = manager.acquire();
  const auto first_index = first.payloadHandle().index;
  manager.release(first);

  auto second = manager.acquire();

  // Assert: Same slot and event reused
  EXPECT_EQ(second.payloadHandle().index, first_index);

  // Cleanup
  manager.release(second);
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0x800)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsEventManagerTypedTest, MovedFromLeaseIsInactive) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x900)},
                                       ::testing::Eq(device));
  }
  manager.configure(mps_rt::MpsEventManager::Config{device, this->getOps(), 1, 1, 1, 1, 1, 1});

  // Arrange
  // Act
  auto lease1 = manager.acquire();
  auto lease2 = std::move(lease1);

  // Assert
  EXPECT_FALSE(lease1);
  EXPECT_TRUE(lease2);

  // Cleanup
  manager.release(lease2);
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0x900)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsEventManagerTypedTest, DestructionReturnsEventToPool) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0xA00)},
                                       ::testing::Eq(device));
  }
  manager.configure(mps_rt::MpsEventManager::Config{device, this->getOps(), 1, 1, 1, 1, 1, 1});

  // Arrange
  std::uint32_t index;
  {
    auto lease = manager.acquire();
    index = lease.payloadHandle().index;
    EXPECT_TRUE(lease);
  }

  // Assert: Can reuse slot
  auto new_lease = manager.acquire();
  EXPECT_EQ(new_lease.payloadHandle().index, index);

  // Cleanup
  manager.release(new_lease);
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0xA00)});
  }
  manager.shutdown();
}

// =============================================================================
// Shutdown Tests
// =============================================================================

TYPED_TEST(MpsEventManagerTypedTest, ShutdownReleasesInitializedEvents) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents(
        {makeEvent(0xB00), makeEvent(0xB01), makeEvent(0xB02)},
        ::testing::Eq(device));
  }
  manager.configure(mps_rt::MpsEventManager::Config{device, this->getOps(), 3, 3, 3, 3, 1, 1});

  // Arrange: Acquire two events
  auto lease1 = manager.acquire();
  auto lease2 = manager.acquire();

  manager.release(lease1);
  manager.release(lease2);

  // Act & Assert: All created events destroyed
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents(
        {makeEvent(0xB00), makeEvent(0xB01), makeEvent(0xB02)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsEventManagerTypedTest, ShutdownWithoutInitializeIsNoOp) {
  auto &manager = this->manager();

  // Act & Assert
  EXPECT_NO_THROW(manager.shutdown());
  EXPECT_NO_THROW(manager.shutdown());
}

TYPED_TEST(MpsEventManagerTypedTest, MultipleShutdownsAreIdempotent) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0xC00)},
                                       ::testing::Eq(device));
  }
  manager.configure(mps_rt::MpsEventManager::Config{device, this->getOps(), 1, 1, 1, 1, 1, 1});

  // Act & Assert
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0xC00)});
  }
  manager.shutdown();
  EXPECT_NO_THROW(manager.shutdown());
  EXPECT_NO_THROW(manager.shutdown());
}

TYPED_TEST(MpsEventManagerTypedTest, ReinitializeResetsPreviousState) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Arrange: First initialization
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0xD00)},
                                       ::testing::Eq(device));
  }
  manager.configure(mps_rt::MpsEventManager::Config{device, this->getOps(), 1, 1, 1, 1, 1, 1});

  auto first_lease = manager.acquire();
  manager.release(first_lease);

  // Act: Reinitialize destroys previous events
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0xD00)});
  }
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0xD10), makeEvent(0xD11)},
                                       ::testing::Eq(device));
  }
  manager.configure(mps_rt::MpsEventManager::Config{device, this->getOps(), 2, 2, 2, 2, 1, 1});

  // Assert: New events available
  auto new_lease = manager.acquire();
  EXPECT_TRUE(new_lease);

  // Cleanup
  manager.release(new_lease);
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0xD10), makeEvent(0xD11)});
  }
  manager.shutdown();
}

// =============================================================================
// Debug State Tests
// =============================================================================

#if ORTEAF_ENABLE_TEST
TYPED_TEST(MpsEventManagerTypedTest, DebugStateReflectsEventState) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0xE00)},
                                       ::testing::Eq(device));
  }
  manager.configure(mps_rt::MpsEventManager::Config{device, this->getOps(), 1, 1, 1, 1, 1, 1});

  // Arrange
  // Act
  auto lease = manager.acquire();
  const auto handle = lease.payloadHandle();

  // Assert
  // Assert
  EXPECT_TRUE(manager.isAliveForTest(handle));
  // Generation check removed as BaseManagerCore + Slot does not use generations

  // Cleanup
  manager.release(lease);
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0xE00)});
  }
  manager.shutdown();
}

// =============================================================================
// Lease Copy/Move Tests with Count Verification
// =============================================================================

TYPED_TEST(MpsEventManagerTypedTest, LeaseCopyIncrementsCount) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0xF00)},
                                       ::testing::Eq(device));
  }
  manager.configure(mps_rt::MpsEventManager::Config{device, this->getOps(), 1, 1, 1, 1, 1, 1});

  // Arrange
  // Act
  auto lease1 = manager.acquire();
  EXPECT_EQ(lease1.count(), 1u);

  auto lease2 = lease1; // Copy

  // Assert: Both valid, count incremented
  EXPECT_TRUE(lease1);
  EXPECT_TRUE(lease2);
  EXPECT_EQ(lease1.count(), 2u);
  EXPECT_EQ(lease2.count(), 2u);

  // Cleanup
  lease1.release();
  EXPECT_EQ(lease2.count(), 1u);
  lease2.release();

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0xF00)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsEventManagerTypedTest, LeaseCopyAssignmentIncrementsCount) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0xF10)},
                                       ::testing::Eq(device));
  }
  manager.configure(mps_rt::MpsEventManager::Config{device, this->getOps(), 1, 1, 1, 1, 1, 1});

  // Arrange
  auto lease1 = manager.acquire();
  decltype(lease1) lease2;

  // Act
  lease2 = lease1; // Copy assignment

  // Assert
  EXPECT_EQ(lease1.count(), 2u);
  EXPECT_EQ(lease2.count(), 2u);

  // Cleanup
  lease1.release();
  lease2.release();

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0xF10)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsEventManagerTypedTest, LeaseMoveDoesNotChangeCount) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0xF20)},
                                       ::testing::Eq(device));
  }
  manager.configure(mps_rt::MpsEventManager::Config{device, this->getOps(), 1, 1, 1, 1, 1, 1});

  // Arrange
  auto lease1 = manager.acquire();
  EXPECT_EQ(lease1.count(), 1u);

  // Act
  auto lease2 = std::move(lease1); // Move

  // Assert: Source invalid, target valid, count unchanged
  EXPECT_FALSE(lease1);
  EXPECT_TRUE(lease2);
  EXPECT_EQ(lease2.count(), 1u);

  // Cleanup
  lease2.release();

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0xF20)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsEventManagerTypedTest, LeaseMoveAssignmentDoesNotChangeCount) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0xF30)},
                                       ::testing::Eq(device));
  }
  manager.configure(mps_rt::MpsEventManager::Config{device, this->getOps(), 1, 1, 1, 1, 1, 1});

  // Arrange
  auto lease1 = manager.acquire();
  decltype(lease1) lease2;

  // Act
  lease2 = std::move(lease1); // Move assignment

  // Assert
  EXPECT_FALSE(lease1);
  EXPECT_TRUE(lease2);
  EXPECT_EQ(lease2.count(), 1u);

  // Cleanup
  lease2.release();

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0xF30)});
  }
  manager.shutdown();
}

#endif

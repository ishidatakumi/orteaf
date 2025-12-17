#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>

#include <orteaf/internal/diagnostics/error/error.h>
#include <orteaf/internal/runtime/mps/manager/mps_event_manager.h>
#include <tests/internal/runtime/mps/manager/testing/backend_ops_provider.h>
#include <tests/internal/runtime/mps/manager/testing/manager_test_fixture.h>
#include <tests/internal/testing/error_assert.h>

namespace base = orteaf::internal::base;
namespace diag_error = orteaf::internal::diagnostics::error;
namespace mps_rt = orteaf::internal::runtime::mps::manager;
namespace mps_wrapper = orteaf::internal::runtime::mps::platform::wrapper;
namespace testing_mps = orteaf::tests::runtime::mps::testing;

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
              [&] { manager.initialize(nullptr, this->getOps(), 1); });
}

TYPED_TEST(MpsEventManagerTypedTest, InitializeRejectsNullOps) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { manager.initialize(device, nullptr, 1); });
}

TYPED_TEST(MpsEventManagerTypedTest, InitializeRejectsExcessiveCapacity) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  const std::size_t excessive =
      static_cast<std::size_t>(base::EventHandle::invalid_index()) + 1;

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { manager.initialize(device, this->getOps(), excessive); });
}

TYPED_TEST(MpsEventManagerTypedTest, OperationsBeforeInitializationThrow) {
  auto &manager = this->manager();

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.acquire(); });
}

TYPED_TEST(MpsEventManagerTypedTest, InitializeDoesNotEagerlyCreateEvents) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Arrange: Initialize should NOT create events (lazy allocation)
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({}, ::testing::Eq(device));
  }
  manager.initialize(device, this->getOps(), 2);

  // Act: First acquire creates event
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x100)},
                                       ::testing::Eq(device));
  }
  auto first = manager.acquire();
  EXPECT_TRUE(first);

  // Act: Second acquire creates another event
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x101)},
                                       ::testing::Eq(device));
  }
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
  manager.initialize(device, this->getOps(), 0);

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
  manager.initialize(device, this->getOps(), 1);

  // Arrange
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x300)},
                                       ::testing::Eq(device));
  }

  // Act
  auto lease = manager.acquire();

  // Assert
  EXPECT_TRUE(lease);
  EXPECT_NE(lease.pointer(), nullptr);
  EXPECT_TRUE(lease.handle().isValid());

  // Cleanup
  manager.release(lease);
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0x300)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsEventManagerTypedTest, AcquireByHandleReturnsValidLease) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  manager.initialize(device, this->getOps(), 1);

  // Arrange
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x400)},
                                       ::testing::Eq(device));
  }

  // Act
  auto first_lease = manager.acquire();
  EXPECT_TRUE(first_lease);
  const auto handle = first_lease.handle();

  auto second_lease = manager.acquire(handle);

  // Assert: Same event, ref count incremented
  EXPECT_TRUE(second_lease);
  EXPECT_EQ(second_lease.handle().index, handle.index);
  EXPECT_EQ(second_lease.handle().generation, handle.generation);
  EXPECT_EQ(second_lease.pointer(), first_lease.pointer());

  // Cleanup
  manager.release(first_lease);
  manager.release(second_lease);
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0x400)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsEventManagerTypedTest, AcquireByInvalidHandleThrows) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  manager.initialize(device, this->getOps(), 1);

  const auto invalid_handle = base::EventHandle{999};

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::OutOfRange,
              [&] { (void)manager.acquire(invalid_handle); });

  manager.shutdown();
}

TYPED_TEST(MpsEventManagerTypedTest, AcquireByStaleHandleThrows) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  manager.initialize(device, this->getOps(), 1);

  // Arrange
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x600)},
                                       ::testing::Eq(device));
  }

  auto lease = manager.acquire();
  const auto handle = lease.handle();
  manager.release(lease);

  // Act: Reacquire reuses slot/event
  auto new_lease = manager.acquire();
  manager.release(new_lease);

  // Assert: Old handle is stale
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.acquire(handle); });

  // Cleanup
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0x600)});
  }
  manager.shutdown();
}

// =============================================================================
// Release Tests
// =============================================================================

TYPED_TEST(MpsEventManagerTypedTest, ReleaseDecrementsRefCount) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  manager.initialize(device, this->getOps(), 1);

  // Arrange
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x700)},
                                       ::testing::Eq(device));
  }

  auto lease1 = manager.acquire();
  const auto handle = lease1.handle();
  auto lease2 = manager.acquire(handle);

  // Act: Release one, event still in use
  manager.release(lease1);

  // Assert: Can still acquire by handle
  auto lease3 = manager.acquire(handle);
  EXPECT_TRUE(lease3);

  // Cleanup
  manager.release(lease2);
  manager.release(lease3);

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0x700)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsEventManagerTypedTest, EventRecyclingReusesSlots) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  manager.initialize(device, this->getOps(), 1);

  // Arrange
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x800)},
                                       ::testing::Eq(device));
  }

  // Act
  auto first = manager.acquire();
  const auto first_index = first.handle().index;
  manager.release(first);

  auto second = manager.acquire();

  // Assert: Same slot and event reused
  EXPECT_EQ(second.handle().index, first_index);

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
  manager.initialize(device, this->getOps(), 1);

  // Arrange
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x900)},
                                       ::testing::Eq(device));
  }

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
  manager.initialize(device, this->getOps(), 1);

  // Arrange
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0xA00)},
                                       ::testing::Eq(device));
  }

  std::uint32_t index;
  {
    auto lease = manager.acquire();
    index = lease.handle().index;
    EXPECT_TRUE(lease);
  }

  // Assert: Can reuse slot
  auto new_lease = manager.acquire();
  EXPECT_EQ(new_lease.handle().index, index);

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
  manager.initialize(device, this->getOps(), 3);

  // Arrange: Create 2 events
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0xB00)},
                                       ::testing::Eq(device));
  }
  auto lease1 = manager.acquire();

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0xB01)},
                                       ::testing::Eq(device));
  }
  auto lease2 = manager.acquire();

  manager.release(lease1);
  manager.release(lease2);

  // Act & Assert: Only 2 created events destroyed (3rd slot unused)
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0xB00), makeEvent(0xB01)});
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
  manager.initialize(device, this->getOps(), 1);

  // Act & Assert
  manager.shutdown();
  EXPECT_NO_THROW(manager.shutdown());
  EXPECT_NO_THROW(manager.shutdown());
}

TYPED_TEST(MpsEventManagerTypedTest, ReinitializeResetsPreviousState) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Arrange: First initialization
  manager.initialize(device, this->getOps(), 1);

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0xD00)},
                                       ::testing::Eq(device));
  }
  auto first_lease = manager.acquire();
  manager.release(first_lease);

  // Act: Reinitialize destroys previous events
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0xD00)});
  }
  manager.initialize(device, this->getOps(), 2);

  // Assert: New event created
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0xD10)},
                                       ::testing::Eq(device));
  }
  auto new_lease = manager.acquire();
  EXPECT_TRUE(new_lease);

  // Cleanup
  manager.release(new_lease);
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0xD10)});
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
  manager.initialize(device, this->getOps(), 1);

  // Arrange
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0xE00)},
                                       ::testing::Eq(device));
  }

  // Act
  auto lease = manager.acquire();
  const auto handle = lease.handle();

  // Assert
  // Assert
  const auto &snapshot = manager.controlBlockForTest(handle.index);
  EXPECT_TRUE(snapshot.isAlive());
  // Generation check removed as BaseManagerCore + Slot does not use generations

  // Cleanup
  manager.release(lease);
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0xE00)});
  }
  manager.shutdown();
}
#endif

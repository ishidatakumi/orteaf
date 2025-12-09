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

mps_wrapper::MPSDevice_t makeDevice(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MPSDevice_t>(value);
}

mps_wrapper::MPSEvent_t makeEvent(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MPSEvent_t>(value);
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

TYPED_TEST(MpsEventManagerTypedTest, InitializeRejectsNullDevice) {
  auto &manager = this->manager();
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { manager.initialize(nullptr, this->getOps(), 1); });
}

TYPED_TEST(MpsEventManagerTypedTest, InitializeRejectsNullOps) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { manager.initialize(device, nullptr, 1); });
}

TYPED_TEST(MpsEventManagerTypedTest, InitializeRejectsExcessiveCapacity) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  // Ensure we do the addition in a type that can hold the value without
  // overflow if size_t > 32-bit
  const std::size_t excessive =
      static_cast<std::size_t>(base::EventHandle::invalid_index()) + 1;
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { manager.initialize(device, this->getOps(), excessive); });
}

TYPED_TEST(MpsEventManagerTypedTest, OperationsBeforeInitializationThrow) {
  auto &manager = this->manager();
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.acquire(); });
}

TYPED_TEST(MpsEventManagerTypedTest, InitializeDoesNotEagerlyCreateEvents) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  // Initialize should NOT create events (lazy allocation)
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({}, ::testing::Eq(device));
  }
  manager.initialize(device, this->getOps(), 2);

  // First acquire should create event
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x100)},
                                       ::testing::Eq(device));
  }
  auto first = manager.acquire();
  EXPECT_TRUE(first);

  // Second acquire should create another event
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x101)},
                                       ::testing::Eq(device));
  }
  auto second = manager.acquire();
  EXPECT_TRUE(second);

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
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({}, ::testing::Eq(device));
  }
  manager.initialize(device, this->getOps(), 0);

  // Should grow on demand
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x200)},
                                       ::testing::Eq(device));
  }
  auto event = manager.acquire();
  EXPECT_TRUE(event);

  manager.release(event);
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0x200)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsEventManagerTypedTest, AcquireReturnsValidLease) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  manager.initialize(device, this->getOps(), 1);

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x300)},
                                       ::testing::Eq(device));
  }
  auto lease = manager.acquire();
  EXPECT_TRUE(lease);
  EXPECT_NE(lease.pointer(), nullptr);
  EXPECT_TRUE(lease.handle().isValid());

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

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x400)},
                                       ::testing::Eq(device));
  }
  auto first_lease = manager.acquire();
  EXPECT_TRUE(first_lease);
  const auto handle = first_lease.handle();

  // Acquire by handle should increment ref count
  auto second_lease = manager.acquire(handle);
  EXPECT_TRUE(second_lease);
  EXPECT_EQ(second_lease.handle().index, handle.index);
  EXPECT_EQ(second_lease.handle().generation, handle.generation);
  EXPECT_EQ(second_lease.pointer(), first_lease.pointer());

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
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { (void)manager.acquire(invalid_handle); });

  // No events created, so no destruction expected
  manager.shutdown();
}

TYPED_TEST(MpsEventManagerTypedTest, AcquireByStaleHandleThrows) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  manager.initialize(device, this->getOps(), 1);

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x600)},
                                       ::testing::Eq(device));
  }
  auto lease = manager.acquire();
  const auto handle = lease.handle();
  manager.release(lease);

  // Now acquire again. It should reuse the slot.
  // The event object itself is reused (not destroyed).
  auto new_lease = manager.acquire();
  // Implicitly: createEvents expected NOT called (reused).

  manager.release(new_lease);

  // Old handle should be stale
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.acquire(handle); });

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0x600)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsEventManagerTypedTest, ReleaseDecrementsRefCount) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  manager.initialize(device, this->getOps(), 1);

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x700)},
                                       ::testing::Eq(device));
  }
  auto lease1 = manager.acquire();
  const auto handle = lease1.handle();
  auto lease2 = manager.acquire(handle);

  // Release first lease, event should still be in use
  manager.release(lease1);

  // Should still be able to acquire by handle
  auto lease3 = manager.acquire(handle);
  EXPECT_TRUE(lease3);

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

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x800)},
                                       ::testing::Eq(device));
  }
  auto first = manager.acquire();
  const auto first_index = first.handle().index;
  manager.release(first);

  // Should reuse the same slot AND the same event
  auto second = manager.acquire();
  EXPECT_EQ(second.handle().index, first_index);

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

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0x900)},
                                       ::testing::Eq(device));
  }
  auto lease1 = manager.acquire();
  auto lease2 = std::move(lease1);

  EXPECT_FALSE(lease1);
  EXPECT_TRUE(lease2);

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

  // Should be able to reuse the slot
  auto new_lease = manager.acquire();
  EXPECT_EQ(new_lease.handle().index, index);

  manager.release(new_lease);
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0xA00)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsEventManagerTypedTest, ShutdownReleasesInitializedEvents) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  manager.initialize(device, this->getOps(), 3);

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

  // Only the 2 created events should be destroyed. The 3rd slot was never
  // used/created.
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0xB00), makeEvent(0xB01)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsEventManagerTypedTest, ShutdownWithoutInitializeIsNoOp) {
  auto &manager = this->manager();
  EXPECT_NO_THROW(manager.shutdown());
  EXPECT_NO_THROW(manager.shutdown());
}

TYPED_TEST(MpsEventManagerTypedTest, MultipleShutdownsAreIdempotent) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  manager.initialize(device, this->getOps(), 1);

  // No events created, so no destruction expectation
  manager.shutdown();

  EXPECT_NO_THROW(manager.shutdown());
  EXPECT_NO_THROW(manager.shutdown());
}

TYPED_TEST(MpsEventManagerTypedTest, ReinitializeResetsPreviousState) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  manager.initialize(device, this->getOps(), 1);

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0xD00)},
                                       ::testing::Eq(device));
  }
  auto first_lease = manager.acquire();
  manager.release(first_lease);

  // initialize() calls shutdown(), which destroys extant events
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0xD00)});
  }
  manager.initialize(device, this->getOps(), 2);

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0xD10)},
                                       ::testing::Eq(device));
  }
  auto new_lease = manager.acquire();
  EXPECT_TRUE(new_lease);

  manager.release(new_lease);
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0xD10)});
  }
  manager.shutdown();
}

#if ORTEAF_ENABLE_TEST
TYPED_TEST(MpsEventManagerTypedTest, DebugStateReflectsEventState) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  manager.initialize(device, this->getOps(), 1);

  if constexpr (TypeParam::is_mock) {
    this->adapter().expectCreateEvents({makeEvent(0xE00)},
                                       ::testing::Eq(device));
  }
  auto lease = manager.acquire();
  const auto handle = lease.handle();

  const auto snapshot = manager.debugState(handle);
  EXPECT_TRUE(snapshot.alive);
  EXPECT_EQ(snapshot.generation, handle.generation);

  manager.release(lease);
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyEvents({makeEvent(0xE00)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsEventManagerTypedTest,
           DebugStateForInvalidHandleReturnsMaxGeneration) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  manager.initialize(device, this->getOps(), 1);

  const auto invalid_handle = base::EventHandle{999};
  const auto snapshot = manager.debugState(invalid_handle);
  EXPECT_EQ(snapshot.generation, std::numeric_limits<std::uint32_t>::max());

  // No events created
  manager.shutdown();
}
#endif

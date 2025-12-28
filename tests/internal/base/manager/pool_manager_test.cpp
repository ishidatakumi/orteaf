#include "orteaf/internal/base/manager/pool_manager.h"

#include <gtest/gtest.h>

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/lease/control_block/shared.h"
#include "orteaf/internal/base/pool/slot_pool.h"
#include "orteaf/internal/base/pool/with_control_block_binding.h"
#include "tests/internal/testing/error_assert.h"

namespace {

struct PayloadTag {};
using PayloadHandle =
    ::orteaf::internal::base::Handle<PayloadTag, std::uint32_t, std::uint8_t>;

struct ControlBlockTag {};

struct DummyPayload {
  int value{0};
};

struct DummyPayloadTraits {
  using Payload = DummyPayload;
  using Handle = PayloadHandle;
  struct Request {};
  struct Context {};

  static bool create(Payload &payload, const Request &, const Context &) {
    payload.value = 1;
    return true;
  }

  static void destroy(Payload &payload, const Request &, const Context &) {
    payload.value = 0;
  }
};

using PayloadPool = ::orteaf::internal::base::pool::SlotPool<DummyPayloadTraits>;
using ControlBlock = ::orteaf::internal::base::SharedControlBlock<
    PayloadHandle, DummyPayload, PayloadPool>;

struct DummyManagerTraits {
  using PayloadPool = ::orteaf::internal::base::pool::SlotPool<DummyPayloadTraits>;
  using ControlBlock = ::orteaf::internal::base::SharedControlBlock<
      PayloadHandle, DummyPayload, PayloadPool>;
  struct ControlBlockTag {};
  using PayloadHandle = ::PayloadHandle;
  static constexpr const char *Name = "DummyManager";
};

using PoolManager =
    ::orteaf::internal::base::PoolManager<DummyManagerTraits>;

struct BoundControlBlockTag {};
using BoundControlBlockHandle =
    ::orteaf::internal::base::pool::ControlBlockHandle<BoundControlBlockTag>;
using BoundPayloadPool = ::orteaf::internal::base::pool::WithControlBlockBinding<
    PayloadPool, BoundControlBlockHandle>;
using BoundControlBlock = ::orteaf::internal::base::SharedControlBlock<
    PayloadHandle, DummyPayload, BoundPayloadPool>;

struct BoundManagerTraits {
  using PayloadPool = BoundPayloadPool;
  using ControlBlock = BoundControlBlock;
  using ControlBlockTag = BoundControlBlockTag;
  using PayloadHandle = ::PayloadHandle;
  static constexpr const char *Name = "BoundManager";
};

using BoundPoolManager =
    ::orteaf::internal::base::PoolManager<BoundManagerTraits>;

PoolManager::Config makeBaseConfig() {
  PoolManager::Config config{};
  config.control_block_capacity = 2;
  config.control_block_block_size = 2;
  config.control_block_growth_chunk_size = 1;
  config.payload_growth_chunk_size = 1;
  config.payload_capacity = 2;
  config.payload_block_size = 2;
  return config;
}

TEST(PoolManager, InitiallyNotConfigured) {
  PoolManager manager;
  EXPECT_FALSE(manager.isConfigured());
}

TEST(PoolManager, EnsureConfiguredThrowsWhenNotConfigured) {
  PoolManager manager;
  ::orteaf::tests::ExpectErrorMessage(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
      {"DummyManager", "has not been configured"},
      [&manager] { manager.ensureConfigured(); });
}

TEST(PoolManager, ConfigureRejectsZeroControlBlockBlockSize) {
  PoolManager manager;
  auto config = makeBaseConfig();
  config.control_block_block_size = 0;
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  ::orteaf::tests::ExpectErrorMessage(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
      {"DummyManager", "control block size must be > 0"},
      [&manager, &config, &req, &ctx] { manager.configure(config, req, ctx); });
}

TEST(PoolManager, ConfigureRejectsZeroControlBlockGrowthChunkSize) {
  PoolManager manager;
  auto config = makeBaseConfig();
  config.control_block_growth_chunk_size = 0;
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  ::orteaf::tests::ExpectErrorMessage(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
      {"DummyManager", "control block growth chunk size must be > 0"},
      [&manager, &config, &req, &ctx] { manager.configure(config, req, ctx); });
}

TEST(PoolManager, ConfigureRejectsZeroPayloadGrowthChunkSize) {
  PoolManager manager;
  auto config = makeBaseConfig();
  config.payload_growth_chunk_size = 0;
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  ::orteaf::tests::ExpectErrorMessage(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
      {"DummyManager", "payload growth chunk size must be > 0"},
      [&manager, &config, &req, &ctx] { manager.configure(config, req, ctx); });
}

TEST(PoolManager, ConfigureRejectsZeroPayloadBlockSize) {
  PoolManager manager;
  auto config = makeBaseConfig();
  config.payload_block_size = 0;
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  ::orteaf::tests::ExpectErrorMessage(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
      {"DummyManager", "payload block size must be > 0"},
      [&manager, &config, &req, &ctx] { manager.configure(config, req, ctx); });
}

TEST(PoolManager, ConfigureMarksManagerConfigured) {
  PoolManager manager;
  auto config = makeBaseConfig();
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  EXPECT_TRUE(manager.isConfigured());
}

TEST(PoolManager, SetControlBlockBlockSizeRejectsZero) {
  PoolManager manager;
  ::orteaf::tests::ExpectErrorMessage(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
      {"DummyManager", "control block size must be > 0"},
      [&manager] { manager.setControlBlockBlockSize(0); });
}

TEST(PoolManager, SetControlBlockBlockSizeAcceptsNonZero) {
  PoolManager manager;
  EXPECT_NO_THROW(manager.setControlBlockBlockSize(2));
}

TEST(PoolManager, SetControlBlockBlockSizeRejectsWhenLeaseActive) {
  PoolManager manager;
  auto config = makeBaseConfig();
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  auto handle = manager.reserveUncreatedPayloadOrGrow();
  EXPECT_TRUE(handle.isValid());
  EXPECT_TRUE(manager.emplacePayload(handle, req, ctx));

  auto lease = manager.acquireStrongLease(handle);
  EXPECT_TRUE(lease);

  ::orteaf::tests::ExpectErrorMessage(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
      {"DummyManager", "shutdown aborted due to active leases"},
      [&manager] { manager.setControlBlockBlockSize(3); });
}

TEST(PoolManager, SetControlBlockBlockSizeAcceptsAfterLeaseReleased) {
  PoolManager manager;
  auto config = makeBaseConfig();
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  auto handle = manager.reserveUncreatedPayloadOrGrow();
  EXPECT_TRUE(handle.isValid());
  EXPECT_TRUE(manager.emplacePayload(handle, req, ctx));

  {
    auto lease = manager.acquireStrongLease(handle);
    EXPECT_TRUE(lease);
  }

  EXPECT_NO_THROW(manager.setControlBlockBlockSize(3));
}

TEST(PoolManager, SetPayloadBlockSizeRejectsZero) {
  PoolManager manager;
  ::orteaf::tests::ExpectErrorMessage(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
      {"DummyManager", "payload block size must be > 0"},
      [&manager] { manager.setPayloadBlockSize(0); });
}

TEST(PoolManager, SetPayloadBlockSizeAcceptsNonZero) {
  PoolManager manager;
  EXPECT_NO_THROW(manager.setPayloadBlockSize(2));
}

TEST(PoolManager, SetPayloadBlockSizeRejectsWhenLeaseActive) {
  PoolManager manager;
  auto config = makeBaseConfig();
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  auto handle = manager.reserveUncreatedPayloadOrGrow();
  EXPECT_TRUE(handle.isValid());
  EXPECT_TRUE(manager.emplacePayload(handle, req, ctx));

  auto lease = manager.acquireStrongLease(handle);
  EXPECT_TRUE(lease);

  ::orteaf::tests::ExpectErrorMessage(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
      {"DummyManager", "teardown aborted due to active strong references"},
      [&manager] { manager.setPayloadBlockSize(3); });
}

TEST(PoolManager, SetPayloadBlockSizeAcceptsAfterLeaseReleased) {
  PoolManager manager;
  auto config = makeBaseConfig();
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  auto handle = manager.reserveUncreatedPayloadOrGrow();
  EXPECT_TRUE(handle.isValid());
  EXPECT_TRUE(manager.emplacePayload(handle, req, ctx));

  {
    auto lease = manager.acquireStrongLease(handle);
    EXPECT_TRUE(lease);
  }

  EXPECT_NO_THROW(manager.setPayloadBlockSize(3));
}

TEST(PoolManager, ConfigureRejectsControlBlockSizeChangeWhenLeaseActive) {
  PoolManager manager;
  auto config = makeBaseConfig();
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  auto handle = manager.reserveUncreatedPayloadOrGrow();
  EXPECT_TRUE(handle.isValid());
  EXPECT_TRUE(manager.emplacePayload(handle, req, ctx));

  auto lease = manager.acquireWeakLease(handle);
  EXPECT_TRUE(lease);

  auto updated = config;
  updated.control_block_block_size = config.control_block_block_size + 1;
  ::orteaf::tests::ExpectErrorMessage(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
      {"DummyManager", "shutdown aborted due to active leases"},
      [&manager, &updated, &req, &ctx] { manager.configure(updated, req, ctx); });
}

TEST(PoolManager, ConfigureAcceptsControlBlockSizeChangeAfterLeaseReleased) {
  PoolManager manager;
  auto config = makeBaseConfig();
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  auto handle = manager.reserveUncreatedPayloadOrGrow();
  EXPECT_TRUE(handle.isValid());
  EXPECT_TRUE(manager.emplacePayload(handle, req, ctx));

  {
    auto lease = manager.acquireWeakLease(handle);
    EXPECT_TRUE(lease);
  }

  auto updated = config;
  updated.control_block_block_size = config.control_block_block_size + 1;
  EXPECT_NO_THROW(manager.configure(updated, req, ctx));
}

TEST(PoolManager, ConfigureRejectsPayloadBlockSizeChangeWhenStrongLeaseActive) {
  PoolManager manager;
  auto config = makeBaseConfig();
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  auto handle = manager.reserveUncreatedPayloadOrGrow();
  EXPECT_TRUE(handle.isValid());
  EXPECT_TRUE(manager.emplacePayload(handle, req, ctx));

  auto lease = manager.acquireStrongLease(handle);
  EXPECT_TRUE(lease);

  auto updated = config;
  updated.payload_block_size = config.payload_block_size + 1;
  ::orteaf::tests::ExpectErrorMessage(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
      {"DummyManager", "teardown aborted due to active strong references"},
      [&manager, &updated, &req, &ctx] { manager.configure(updated, req, ctx); });
}

TEST(PoolManager, ConfigureAcceptsPayloadBlockSizeChangeAfterLeaseReleased) {
  PoolManager manager;
  auto config = makeBaseConfig();
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  auto handle = manager.reserveUncreatedPayloadOrGrow();
  EXPECT_TRUE(handle.isValid());
  EXPECT_TRUE(manager.emplacePayload(handle, req, ctx));

  {
    auto lease = manager.acquireStrongLease(handle);
    EXPECT_TRUE(lease);
  }

  auto updated = config;
  updated.payload_block_size = config.payload_block_size + 1;
  EXPECT_NO_THROW(manager.configure(updated, req, ctx));
}

TEST(PoolManager, ShutdownRejectsWhenStrongLeaseActive) {
  PoolManager manager;
  auto config = makeBaseConfig();
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  auto handle = manager.reserveUncreatedPayloadOrGrow();
  EXPECT_TRUE(handle.isValid());
  EXPECT_TRUE(manager.emplacePayload(handle, req, ctx));

  auto lease = manager.acquireStrongLease(handle);
  EXPECT_TRUE(lease);

  ::orteaf::tests::ExpectErrorMessage(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
      {"DummyManager", "teardown aborted due to active strong references"},
      [&manager, &req, &ctx] { manager.shutdown(req, ctx); });
}

TEST(PoolManager, ShutdownRejectsWhenWeakLeaseActive) {
  PoolManager manager;
  auto config = makeBaseConfig();
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  auto handle = manager.reserveUncreatedPayloadOrGrow();
  EXPECT_TRUE(handle.isValid());
  EXPECT_TRUE(manager.emplacePayload(handle, req, ctx));

  auto strong = manager.acquireStrongLease(handle);
  EXPECT_TRUE(strong);
  auto weak = manager.acquireWeakLease(handle);
  EXPECT_TRUE(weak);
  strong.release();

  ::orteaf::tests::ExpectErrorMessage(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
      {"DummyManager", "shutdown aborted due to active leases"},
      [&manager, &req, &ctx] { manager.shutdown(req, ctx); });
}

TEST(PoolManager, ShutdownAcceptsAfterLeasesReleased) {
  PoolManager manager;
  auto config = makeBaseConfig();
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  auto handle = manager.reserveUncreatedPayloadOrGrow();
  EXPECT_TRUE(handle.isValid());
  EXPECT_TRUE(manager.emplacePayload(handle, req, ctx));

  {
    auto strong = manager.acquireStrongLease(handle);
    EXPECT_TRUE(strong);
    auto weak = manager.acquireWeakLease(handle);
    EXPECT_TRUE(weak);
  }

  EXPECT_NO_THROW(manager.shutdown(req, ctx));
  EXPECT_FALSE(manager.isConfigured());
}

TEST(PoolManager, ControlBlockGrowthChunkSizeDefaultsToOne) {
  PoolManager manager;
  EXPECT_EQ(manager.controlBlockGrowthChunkSize(), 1u);
}

TEST(PoolManager, SetControlBlockGrowthChunkSizeRejectsZero) {
  PoolManager manager;
  ::orteaf::tests::ExpectErrorMessage(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
      {"DummyManager", "control block growth chunk size must be > 0"},
      [&manager] { manager.setControlBlockGrowthChunkSize(0); });
}

TEST(PoolManager, SetControlBlockGrowthChunkSizeUpdatesValue) {
  PoolManager manager;
  manager.setControlBlockGrowthChunkSize(3);
  EXPECT_EQ(manager.controlBlockGrowthChunkSize(), 3u);
}

TEST(PoolManager, PayloadGrowthChunkSizeDefaultsToOne) {
  PoolManager manager;
  EXPECT_EQ(manager.payloadGrowthChunkSize(), 1u);
}

TEST(PoolManager, SetPayloadGrowthChunkSizeRejectsZero) {
  PoolManager manager;
  ::orteaf::tests::ExpectErrorMessage(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
      {"DummyManager", "payload growth chunk size must be > 0"},
      [&manager] { manager.setPayloadGrowthChunkSize(0); });
}

TEST(PoolManager, SetPayloadGrowthChunkSizeUpdatesValue) {
  PoolManager manager;
  manager.setPayloadGrowthChunkSize(4);
  EXPECT_EQ(manager.payloadGrowthChunkSize(), 4u);
}

TEST(PoolManager, IsAliveReturnsFalseWhenNotConfigured) {
  PoolManager manager;
  EXPECT_FALSE(manager.isAlive(PayloadHandle::invalid()));
}

TEST(PoolManager, IsAliveReturnsFalseForInvalidHandle) {
  PoolManager manager;
  auto config = makeBaseConfig();
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  EXPECT_FALSE(manager.isAlive(PayloadHandle::invalid()));
}

TEST(PoolManager, IsAliveReturnsFalseForUncreatedPayload) {
  PoolManager manager;
  auto config = makeBaseConfig();
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  auto handle = manager.reserveUncreatedPayloadOrGrow();
  EXPECT_TRUE(handle.isValid());
  EXPECT_FALSE(manager.isAlive(handle));
}

TEST(PoolManager, IsAliveReturnsTrueForCreatedPayload) {
  PoolManager manager;
  auto config = makeBaseConfig();
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  auto handle = manager.reserveUncreatedPayloadOrGrow();
  EXPECT_TRUE(handle.isValid());
  EXPECT_TRUE(manager.emplacePayload(handle, req, ctx));
  EXPECT_TRUE(manager.isAlive(handle));
}

TEST(PoolManager, EmplacePayloadReturnsTrueAndCreatesPayload) {
  PoolManager manager;
  auto config = makeBaseConfig();
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  auto handle = manager.reserveUncreatedPayloadOrGrow();
  EXPECT_TRUE(handle.isValid());
  EXPECT_TRUE(manager.emplacePayload(handle, req, ctx));
  EXPECT_TRUE(manager.isAlive(handle));
}

TEST(PoolManager, EmplacePayloadReturnsFalseForInvalidHandle) {
  PoolManager manager;
  auto config = makeBaseConfig();
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  EXPECT_FALSE(manager.emplacePayload(PayloadHandle::invalid(), req, ctx));
}

TEST(PoolManager, EmplacePayloadReturnsFalseWhenAlreadyCreated) {
  PoolManager manager;
  auto config = makeBaseConfig();
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  auto handle = manager.reserveUncreatedPayloadOrGrow();
  EXPECT_TRUE(handle.isValid());
  EXPECT_TRUE(manager.emplacePayload(handle, req, ctx));
  EXPECT_FALSE(manager.emplacePayload(handle, req, ctx));
}

TEST(PoolManager, CreateAllPayloadsCreatesAllSlots) {
  PoolManager manager;
  auto config = makeBaseConfig();
  config.payload_capacity = 3;
  config.payload_block_size = 3;
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  EXPECT_TRUE(manager.createAllPayloads(req, ctx));

  for (std::size_t i = 0; i < config.payload_capacity; ++i) {
    auto handle =
        PayloadHandle{static_cast<PayloadHandle::index_type>(i), 0};
    EXPECT_TRUE(manager.isAlive(handle));
  }
}

TEST(PoolManager, ReserveUncreatedPayloadOrGrowReturnsUncreatedHandle) {
  PoolManager manager;
  auto config = makeBaseConfig();
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  auto handle = manager.reserveUncreatedPayloadOrGrow();
  EXPECT_TRUE(handle.isValid());
  EXPECT_FALSE(manager.isAlive(handle));
}

TEST(PoolManager, ReserveUncreatedPayloadOrGrowGrowsWhenExhausted) {
  PoolManager manager;
  auto config = makeBaseConfig();
  config.payload_capacity = 1;
  config.payload_growth_chunk_size = 2;
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  auto first = manager.reserveUncreatedPayloadOrGrow();
  EXPECT_TRUE(first.isValid());
  auto second = manager.reserveUncreatedPayloadOrGrow();
  EXPECT_TRUE(second.isValid());
  EXPECT_NE(second.index, first.index);
  EXPECT_FALSE(manager.isAlive(second));
}

TEST(PoolManager, AcquirePayloadOrGrowAndCreateCreatesPayload) {
  PoolManager manager;
  auto config = makeBaseConfig();
  config.payload_capacity = 0;
  config.payload_block_size = 1;
  config.payload_growth_chunk_size = 1;
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  auto handle = manager.acquirePayloadOrGrowAndCreate(req, ctx);
  EXPECT_TRUE(handle.isValid());
  EXPECT_TRUE(manager.isAlive(handle));
}

TEST(PoolManager, AcquireStrongLeaseRejectsInvalidHandle) {
  PoolManager manager;
  auto config = makeBaseConfig();
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  ::orteaf::tests::ExpectErrorMessage(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
      {"DummyManager", "handle is invalid"},
      [&manager] { manager.acquireStrongLease(PayloadHandle::invalid()); });
}

TEST(PoolManager, AcquireWeakLeaseRejectsInvalidHandle) {
  PoolManager manager;
  auto config = makeBaseConfig();
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  ::orteaf::tests::ExpectErrorMessage(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
      {"DummyManager", "handle is invalid"},
      [&manager] { manager.acquireWeakLease(PayloadHandle::invalid()); });
}

TEST(PoolManager, AcquireStrongLeaseRejectsUncreatedPayload) {
  PoolManager manager;
  auto config = makeBaseConfig();
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  auto handle = manager.reserveUncreatedPayloadOrGrow();
  EXPECT_TRUE(handle.isValid());
  ::orteaf::tests::ExpectErrorMessage(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
      {"DummyManager", "payload is unavailable"},
      [&manager, handle] { manager.acquireStrongLease(handle); });
}

TEST(PoolManager, AcquireWeakLeaseRejectsUncreatedPayload) {
  PoolManager manager;
  auto config = makeBaseConfig();
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  auto handle = manager.reserveUncreatedPayloadOrGrow();
  EXPECT_TRUE(handle.isValid());
  ::orteaf::tests::ExpectErrorMessage(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
      {"DummyManager", "payload is unavailable"},
      [&manager, handle] { manager.acquireWeakLease(handle); });
}

TEST(PoolManager, AcquireStrongLeaseReturnsValidLease) {
  PoolManager manager;
  auto config = makeBaseConfig();
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  auto handle = manager.reserveUncreatedPayloadOrGrow();
  EXPECT_TRUE(handle.isValid());
  EXPECT_TRUE(manager.emplacePayload(handle, req, ctx));

  auto lease = manager.acquireStrongLease(handle);
  EXPECT_TRUE(lease);
  EXPECT_EQ(lease.strongCount(), 1u);
  EXPECT_NE(lease.payloadPtr(), nullptr);
  EXPECT_EQ(lease.payloadPtr()->value, 1);
}

TEST(PoolManager, AcquireWeakLeaseReturnsValidLease) {
  PoolManager manager;
  auto config = makeBaseConfig();
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  auto handle = manager.reserveUncreatedPayloadOrGrow();
  EXPECT_TRUE(handle.isValid());
  EXPECT_TRUE(manager.emplacePayload(handle, req, ctx));

  auto lease = manager.acquireWeakLease(handle);
  EXPECT_TRUE(lease);
  EXPECT_EQ(lease.weakCount(), 1u);
  EXPECT_NE(lease.payloadPtr(), nullptr);
  EXPECT_EQ(lease.payloadPtr()->value, 1);
}

TEST(PoolManager, AcquireStrongLeaseRebindsControlBlockAfterRelease) {
  BoundPoolManager manager;
  BoundPoolManager::Config config{};
  config.control_block_capacity = 1;
  config.control_block_block_size = 1;
  config.control_block_growth_chunk_size = 1;
  config.payload_growth_chunk_size = 1;
  config.payload_capacity = 1;
  config.payload_block_size = 1;
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  auto handle = manager.reserveUncreatedPayloadOrGrow();
  EXPECT_TRUE(handle.isValid());
  EXPECT_TRUE(manager.emplacePayload(handle, req, ctx));

  auto lease = manager.acquireStrongLease(handle);
  EXPECT_TRUE(lease);
  auto bound = manager.boundControlBlockForTest(handle);
  EXPECT_TRUE(bound.isValid());
  EXPECT_EQ(bound, lease.handle());

  lease.release();
  auto reacquired_handle = manager.acquirePayloadOrGrowAndCreate(req, ctx);
  EXPECT_TRUE(reacquired_handle.isValid());
  EXPECT_EQ(reacquired_handle.index, handle.index);
  auto bound_after = manager.boundControlBlockForTest(reacquired_handle);
  EXPECT_FALSE(bound_after.isValid());

  auto reused = manager.acquireStrongLease(reacquired_handle);
  EXPECT_TRUE(reused);
  auto rebound = manager.boundControlBlockForTest(reacquired_handle);
  EXPECT_TRUE(rebound.isValid());
  EXPECT_EQ(rebound, reused.handle());
  EXPECT_EQ(reused.strongCount(), 1u);
}

TEST(PoolManager, AcquireStrongLeaseTwiceReusesSameControlBlockAndHandle) {
  BoundPoolManager manager;
  BoundPoolManager::Config config{};
  config.control_block_capacity = 1;
  config.control_block_block_size = 1;
  config.control_block_growth_chunk_size = 1;
  config.payload_growth_chunk_size = 1;
  config.payload_capacity = 1;
  config.payload_block_size = 1;
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  auto handle = manager.reserveUncreatedPayloadOrGrow();
  EXPECT_TRUE(handle.isValid());
  EXPECT_TRUE(manager.emplacePayload(handle, req, ctx));

  auto first = manager.acquireStrongLease(handle);
  auto second = manager.acquireStrongLease(handle);

  EXPECT_TRUE(first);
  EXPECT_TRUE(second);
  EXPECT_EQ(first.handle(), second.handle());
  EXPECT_EQ(first.payloadHandle(), handle);
  EXPECT_EQ(second.payloadHandle(), handle);
}

TEST(PoolManager, AcquireWeakLeaseTwiceReusesSameControlBlockAndHandle) {
  BoundPoolManager manager;
  BoundPoolManager::Config config{};
  config.control_block_capacity = 1;
  config.control_block_block_size = 1;
  config.control_block_growth_chunk_size = 1;
  config.payload_growth_chunk_size = 1;
  config.payload_capacity = 1;
  config.payload_block_size = 1;
  DummyPayloadTraits::Request req{};
  DummyPayloadTraits::Context ctx{};

  manager.configure(config, req, ctx);
  auto handle = manager.reserveUncreatedPayloadOrGrow();
  EXPECT_TRUE(handle.isValid());
  EXPECT_TRUE(manager.emplacePayload(handle, req, ctx));

  auto first = manager.acquireWeakLease(handle);
  auto second = manager.acquireWeakLease(handle);

  EXPECT_TRUE(first);
  EXPECT_TRUE(second);
  EXPECT_EQ(first.handle(), second.handle());
  EXPECT_EQ(first.payloadHandle(), handle);
  EXPECT_EQ(second.payloadHandle(), handle);
}

} // namespace

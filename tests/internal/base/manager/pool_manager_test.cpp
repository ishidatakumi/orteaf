#include "orteaf/internal/base/manager/pool_manager.h"

#include <gtest/gtest.h>

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/lease/control_block/shared.h"
#include "orteaf/internal/base/pool/slot_pool.h"
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

} // namespace

#include "orteaf/internal/base/pool/with_control_block_binding.h"

#include <gtest/gtest.h>

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/pool/fixed_slot_store.h"
#include "orteaf/internal/base/pool/pool_concepts.h"
#include "orteaf/internal/base/pool/slot_pool.h"

namespace {

// =============================================================================
// Test Types
// =============================================================================

struct PayloadTag {};
using PayloadHandle =
    ::orteaf::internal::base::Handle<PayloadTag, std::uint32_t, std::uint8_t>;

struct ControlBlockTag {};
using CBHandle = ::orteaf::internal::base::Handle<ControlBlockTag,
                                                  std::uint32_t, std::uint8_t>;

struct DummyPayload {
  int value{0};
};

struct DummyTraits {
  using Payload = DummyPayload;
  using Handle = PayloadHandle;
  struct Request {};
  struct Context {};

  static bool create(Payload &payload, const Request &, const Context &) {
    payload.value = 42;
    return true;
  }

  static void destroy(Payload &payload, const Request &, const Context &) {
    payload.value = 0;
  }
};

// Type aliases for bound pools
using BaseSlotPool = ::orteaf::internal::base::pool::SlotPool<DummyTraits>;
using BaseFixedSlotStore =
    ::orteaf::internal::base::pool::FixedSlotStore<DummyTraits>;

using BoundSlotPool =
    ::orteaf::internal::base::pool::WithControlBlockBinding<BaseSlotPool,
                                                            CBHandle>;
using BoundFixedSlotStore =
    ::orteaf::internal::base::pool::WithControlBlockBinding<BaseFixedSlotStore,
                                                            CBHandle>;

// =============================================================================
// Concept Tests
// =============================================================================

TEST(WithControlBlockBinding, SlotPoolSatisfiesControlBlockBindableConcept) {
  static_assert(::orteaf::internal::base::pool::ControlBlockBindableConcept<
                    BoundSlotPool>,
                "BoundSlotPool must satisfy ControlBlockBindableConcept");
}

TEST(WithControlBlockBinding,
     FixedSlotStoreSatisfiesControlBlockBindableConcept) {
  static_assert(::orteaf::internal::base::pool::ControlBlockBindableConcept<
                    BoundFixedSlotStore>,
                "BoundFixedSlotStore must satisfy ControlBlockBindableConcept");
}

TEST(WithControlBlockBinding, SlotPoolSatisfiesBoundPoolConcept) {
  static_assert(::orteaf::internal::base::pool::BoundPoolConcept<BoundSlotPool>,
                "BoundSlotPool must satisfy BoundPoolConcept");
}

// =============================================================================
// BoundSlotPool Tests
// =============================================================================

class BoundSlotPoolTest : public ::testing::Test {
protected:
  BoundSlotPool pool;
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  void SetUp() override { pool.configure(BoundSlotPool::Config{3, 3}); }
};

TEST_F(BoundSlotPoolTest, ConfigureInitializesBoundControlBlocks) {
  EXPECT_EQ(pool.boundControlBlocksSize(), 3u);
}

TEST_F(BoundSlotPoolTest, HasBoundControlBlockReturnsFalseInitially) {
  auto ref = pool.reserveUncreated();
  ASSERT_TRUE(ref.valid());
  EXPECT_FALSE(pool.hasBoundControlBlock(ref.handle));
}

TEST_F(BoundSlotPoolTest, BindAndUnbindControlBlock) {
  auto ref = pool.reserveUncreated();
  ASSERT_TRUE(ref.valid());
  EXPECT_TRUE(pool.emplace(ref.handle, req, ctx));

  CBHandle cb_handle{0, 1};
  pool.bindControlBlock(ref.handle, cb_handle);

  EXPECT_TRUE(pool.hasBoundControlBlock(ref.handle));
  EXPECT_EQ(pool.getBoundControlBlock(ref.handle), cb_handle);

  pool.unbindControlBlock(ref.handle);
  EXPECT_FALSE(pool.hasBoundControlBlock(ref.handle));
}

TEST_F(BoundSlotPoolTest, GetBoundControlBlockReturnsInvalidForUnbound) {
  auto ref = pool.reserveUncreated();
  ASSERT_TRUE(ref.valid());
  EXPECT_FALSE(pool.getBoundControlBlock(ref.handle).isValid());
}

TEST_F(BoundSlotPoolTest, ResizeExpandsBoundControlBlocks) {
  pool.resize(5);
  EXPECT_EQ(pool.boundControlBlocksSize(), 5u);
}

TEST_F(BoundSlotPoolTest, ShutdownClearsBoundControlBlocks) {
  auto ref = pool.reserveUncreated();
  ASSERT_TRUE(ref.valid());
  pool.emplace(ref.handle, req, ctx);

  CBHandle cb_handle{0, 1};
  pool.bindControlBlock(ref.handle, cb_handle);

  pool.shutdown(req, ctx);
  EXPECT_EQ(pool.boundControlBlocksSize(), 0u);
}

// =============================================================================
// BoundFixedSlotStore Tests
// =============================================================================

class BoundFixedSlotStoreTest : public ::testing::Test {
protected:
  BoundFixedSlotStore store;
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  void SetUp() override { store.configure(BoundFixedSlotStore::Config{3, 3}); }
};

TEST_F(BoundFixedSlotStoreTest, ConfigureInitializesBoundControlBlocks) {
  EXPECT_EQ(store.boundControlBlocksSize(), 3u);
}

TEST_F(BoundFixedSlotStoreTest, BindAndRetrieveControlBlock) {
  auto ref = store.reserveUncreated();
  ASSERT_TRUE(ref.valid());
  EXPECT_TRUE(store.emplace(ref.handle, req, ctx));

  CBHandle cb_handle{1, 2};
  store.bindControlBlock(ref.handle, cb_handle);

  EXPECT_TRUE(store.hasBoundControlBlock(ref.handle));
  EXPECT_EQ(store.getBoundControlBlock(ref.handle), cb_handle);
}

TEST_F(BoundFixedSlotStoreTest, MultiplePayloadsCanHaveDifferentCBs) {
  store.createAll(req, ctx);

  PayloadHandle h0{0, 0};
  PayloadHandle h1{1, 0};

  CBHandle cb0{10, 0};
  CBHandle cb1{20, 0};

  store.bindControlBlock(h0, cb0);
  store.bindControlBlock(h1, cb1);

  EXPECT_EQ(store.getBoundControlBlock(h0), cb0);
  EXPECT_EQ(store.getBoundControlBlock(h1), cb1);
}

} // namespace

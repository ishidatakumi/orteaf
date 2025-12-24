#include "orteaf/internal/execution/base/pool/slot_pool.h"

#include <gtest/gtest.h>
#include <system_error>
#include <vector>

#include "orteaf/internal/base/handle.h"

namespace {

struct SlotTag {};
using SlotHandle = ::orteaf::internal::base::Handle<SlotTag, std::uint32_t, std::uint8_t>;

struct DummyPayload {
  int value{0};
};

struct DummyTraits {
  using Payload = DummyPayload;
  using Handle = SlotHandle;
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

struct DestroyOnReleaseTraits {
  using Payload = DummyPayload;
  using Handle = SlotHandle;
  struct Request {};
  struct Context {};
  static constexpr bool destroy_on_release = true;

  static bool create(Payload &payload, const Request &, const Context &) {
    payload.value = 9;
    return true;
  }

  static void destroy(Payload &payload, const Request &, const Context &) {
    payload.value = -1;
  }
};

using Pool = ::orteaf::internal::execution::base::pool::SlotPool<DummyTraits>;
using DestroyOnReleasePool =
    ::orteaf::internal::execution::base::pool::SlotPool<DestroyOnReleaseTraits>;

Pool makePool(std::size_t capacity) {
  Pool pool;
  pool.configure(typename Pool::Config{capacity, capacity});
  return pool;
}

DestroyOnReleasePool makeDestroyOnReleasePool(std::size_t capacity) {
  DestroyOnReleasePool pool;
  pool.configure(typename DestroyOnReleasePool::Config{capacity, capacity});
  return pool;
}

TEST(SlotPool, InitializeSetsSizeAndAvailable) {
  auto pool = makePool(3);
  EXPECT_EQ(pool.size(), 3u);
  EXPECT_EQ(pool.available(), 3u);
}

TEST(SlotPool, ReserveReturnsValidSlotRefAndPayloadPtr) {
  auto pool = makePool(2);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  auto ref = pool.reserveUncreated(req, ctx);
  EXPECT_TRUE(ref.valid());
  EXPECT_TRUE(ref.handle.isValid());
  EXPECT_NE(ref.payload_ptr, nullptr);
  EXPECT_EQ(pool.get(ref.handle), ref.payload_ptr);
  EXPECT_EQ(pool.available(), 1u);
}

TEST(SlotPool, TryReserveReturnsInvalidWhenEmpty) {
  auto pool = makePool(1);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  auto first = pool.tryReserveUncreated(req, ctx);
  auto second = pool.tryReserveUncreated(req, ctx);

  EXPECT_TRUE(first.valid());
  EXPECT_FALSE(second.valid());
  EXPECT_EQ(pool.available(), 0u);
}

TEST(SlotPool, ReserveThrowsWhenEmpty) {
  auto pool = makePool(1);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  (void)pool.reserveUncreated(req, ctx);
  EXPECT_THROW(pool.reserveUncreated(req, ctx), std::system_error);
}

TEST(SlotPool, TryAcquireReturnsInvalidWhenNoCreated) {
  auto pool = makePool(1);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  auto ref = pool.tryAcquireCreated(req, ctx);
  EXPECT_FALSE(ref.valid());
}

TEST(SlotPool, AcquireThrowsWhenNoCreated) {
  auto pool = makePool(1);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  EXPECT_THROW(pool.acquireCreated(req, ctx), std::system_error);
}

TEST(SlotPool, AcquireReturnsCreatedSlotAfterRelease) {
  auto pool = makePool(1);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  auto reserved = pool.reserveUncreated(req, ctx);
  EXPECT_TRUE(pool.emplace(reserved.handle, req, ctx));
  EXPECT_TRUE(pool.release(reserved.handle));

  auto ref = pool.acquireCreated(req, ctx);
  EXPECT_TRUE(ref.valid());
}

TEST(SlotPool, ReleaseReturnsSlotToFreelistAndIncrementsGeneration) {
  auto pool = makePool(1);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  auto first = pool.reserveUncreated(req, ctx);
  auto handle = first.handle;
  EXPECT_TRUE(pool.emplace(handle, req, ctx));

  EXPECT_TRUE(pool.release(handle));

  auto second = pool.acquireCreated(req, ctx);
  EXPECT_EQ(second.handle.index, handle.index);
  EXPECT_EQ(static_cast<std::size_t>(second.handle.generation),
            static_cast<std::size_t>(handle.generation) + 1u);
}

TEST(SlotPool, ReleaseRejectsStaleGeneration) {
  auto pool = makePool(1);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  auto first = pool.reserveUncreated(req, ctx);
  auto stale = first.handle;
  EXPECT_TRUE(pool.emplace(stale, req, ctx));

  EXPECT_TRUE(pool.release(stale));
  EXPECT_FALSE(pool.release(stale));
}

TEST(SlotPool, EmplaceUsesTraitsCreateAndSetsCreated) {
  auto pool = makePool(1);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  auto slot = pool.reserveUncreated(req, ctx);
  EXPECT_TRUE(pool.emplace(slot.handle, req, ctx));
  EXPECT_TRUE(pool.isCreated(slot.handle));
  EXPECT_EQ(pool.get(slot.handle)->value, 42);
}

TEST(SlotPool, EmplaceLambdaOverridesTraitsCreate) {
  auto pool = makePool(1);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  auto slot = pool.reserveUncreated(req, ctx);
  EXPECT_TRUE(
      pool.emplace(slot.handle, req, ctx,
                   [](DummyPayload &payload, const DummyTraits::Request &,
                      const DummyTraits::Context &) {
                     payload.value = 7;
                     return true;
                   }));
  EXPECT_TRUE(pool.isCreated(slot.handle));
  EXPECT_EQ(pool.get(slot.handle)->value, 7);
}

TEST(SlotPool, DestroyUsesTraitsDestroyAndClearsCreated) {
  auto pool = makePool(1);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  auto slot = pool.reserveUncreated(req, ctx);
  EXPECT_TRUE(pool.emplace(slot.handle, req, ctx));
  EXPECT_TRUE(pool.destroy(slot.handle, req, ctx));
  EXPECT_FALSE(pool.isCreated(slot.handle));
  EXPECT_EQ(pool.get(slot.handle)->value, 0);
}

TEST(SlotPool, DestroyLambdaOverridesTraitsDestroy) {
  auto pool = makePool(1);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  auto slot = pool.reserveUncreated(req, ctx);
  EXPECT_TRUE(pool.emplace(slot.handle, req, ctx));
  EXPECT_TRUE(
      pool.destroy(slot.handle, req, ctx,
                   [](DummyPayload &payload, const DummyTraits::Request &,
                      const DummyTraits::Context &) {
                     payload.value = -1;
                     return true;
                   }));
  EXPECT_FALSE(pool.isCreated(slot.handle));
  EXPECT_EQ(pool.get(slot.handle)->value, -1);
}

TEST(SlotPool, GetReturnsNullForInvalidHandle) {
  auto pool = makePool(1);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  auto ref = pool.reserveUncreated(req, ctx);
  auto stale = ref.handle;
  EXPECT_TRUE(pool.emplace(stale, req, ctx));

  EXPECT_TRUE(pool.release(stale));
  EXPECT_EQ(pool.get(stale), nullptr);
}

TEST(SlotPool, ReleaseDestroysWhenConfigured) {
  auto pool = makeDestroyOnReleasePool(1);
  DestroyOnReleaseTraits::Request req{};
  DestroyOnReleaseTraits::Context ctx{};

  auto slot = pool.reserveUncreated(req, ctx);
  EXPECT_TRUE(pool.emplace(slot.handle, req, ctx));
  EXPECT_TRUE(pool.release(slot.handle, req, ctx));
  EXPECT_FALSE(pool.isCreated(slot.handle));
  auto re_reserved = pool.reserveUncreated(req, ctx);
  EXPECT_TRUE(re_reserved.valid());
  EXPECT_EQ(re_reserved.payload_ptr->value, -1);
}

TEST(SlotPool, ReleaseFailsWhenNotCreatedInDestroyMode) {
  auto pool = makeDestroyOnReleasePool(1);
  DestroyOnReleaseTraits::Request req{};
  DestroyOnReleaseTraits::Context ctx{};

  auto slot = pool.reserveUncreated(req, ctx);
  EXPECT_FALSE(pool.release(slot.handle, req, ctx));
}

TEST(SlotPool, GrowAddsUncreatedSlots) {
  auto pool = makePool(1);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  pool.configure(typename Pool::Config{3, 3});

  EXPECT_EQ(pool.size(), 3u);
  EXPECT_EQ(pool.available(), 3u);

  auto ref = pool.reserveUncreated(req, ctx);
  EXPECT_TRUE(ref.valid());
  EXPECT_FALSE(pool.isCreated(ref.handle));
}

TEST(SlotPool, GrowAndCreateCreatesNewSlots) {
  auto pool = makePool(1);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  auto reserved = pool.reserveUncreated(req, ctx);
  EXPECT_TRUE(pool.emplace(reserved.handle, req, ctx));
  EXPECT_TRUE(pool.release(reserved.handle));

  const std::size_t old_capacity = pool.configure(typename Pool::Config{3, 3});
  EXPECT_TRUE(pool.createRange(old_capacity, pool.size(), req, ctx));
  EXPECT_FALSE(pool.tryReserveUncreated(req, ctx).valid());

  auto ref = pool.acquireCreated(req, ctx);
  EXPECT_TRUE(ref.valid());
}

TEST(SlotPool, ForEachCreatedVisitsOnlyCreatedSlots) {
  auto pool = makePool(3);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  auto first = pool.reserveUncreated(req, ctx);
  EXPECT_TRUE(pool.emplace(first.handle, req, ctx));

  auto second = pool.reserveUncreated(req, ctx);
  EXPECT_TRUE(pool.emplace(second.handle, req, ctx));
  EXPECT_TRUE(pool.release(second.handle));

  std::vector<std::size_t> indices{};
  std::vector<int> values{};
  pool.forEachCreated([&](std::size_t idx, const DummyPayload &payload) {
    indices.push_back(idx);
    values.push_back(payload.value);
  });

  ASSERT_EQ(indices.size(), 2u);
  EXPECT_EQ(indices[0], static_cast<std::size_t>(first.handle.index));
  EXPECT_EQ(values[0], 42);
}

TEST(SlotPool, InitializeAndCreateCreatesAllSlots) {
  Pool pool;
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  pool.configure(typename Pool::Config{2, 2});
  EXPECT_TRUE(pool.createAll(req, ctx));
  EXPECT_EQ(pool.size(), 2u);
  EXPECT_EQ(pool.available(), 2u);
  EXPECT_FALSE(pool.tryReserveUncreated(req, ctx).valid());
  EXPECT_TRUE(pool.tryAcquireCreated(req, ctx).valid());
}

TEST(SlotPool, ReserveDoesNotChangeSize) {
  Pool pool;
  pool.reserve(4);
  EXPECT_EQ(pool.size(), 0u);
  EXPECT_GE(pool.capacity(), 4u);
}

TEST(SlotPool, ResizeGrowsSlots) {
  Pool pool;
  const std::size_t old_size = pool.resize(3);
  EXPECT_EQ(old_size, 0u);
  EXPECT_EQ(pool.size(), 3u);
  EXPECT_EQ(pool.available(), 3u);
}

} // namespace

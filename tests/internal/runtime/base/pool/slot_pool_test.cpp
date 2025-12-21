#include "orteaf/internal/runtime/base/pool/slot_pool.h"

#include <gtest/gtest.h>
#include <system_error>

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
  struct Config {
    std::size_t capacity{0};
  };

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
  struct Config {
    std::size_t capacity{0};
  };
  static constexpr bool destroy_on_release = true;

  static bool create(Payload &payload, const Request &, const Context &) {
    payload.value = 9;
    return true;
  }

  static void destroy(Payload &payload, const Request &, const Context &) {
    payload.value = -1;
  }
};

using Pool = ::orteaf::internal::runtime::base::pool::SlotPool<DummyTraits>;
using DestroyOnReleasePool =
    ::orteaf::internal::runtime::base::pool::SlotPool<DestroyOnReleaseTraits>;

Pool makePool(std::size_t capacity) {
  Pool pool;
  pool.initialize(typename DummyTraits::Config{capacity});
  return pool;
}

DestroyOnReleasePool makeDestroyOnReleasePool(std::size_t capacity) {
  DestroyOnReleasePool pool;
  pool.initialize(typename DestroyOnReleaseTraits::Config{capacity});
  return pool;
}

TEST(SlotPool, InitializeSetsCapacityAndAvailable) {
  auto pool = makePool(3);
  EXPECT_EQ(pool.capacity(), 3u);
  EXPECT_EQ(pool.available(), 3u);
}

TEST(SlotPool, ReserveReturnsValidSlotRefAndPayloadPtr) {
  auto pool = makePool(2);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  auto ref = pool.reserve(req, ctx);
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

  auto first = pool.tryReserve(req, ctx);
  auto second = pool.tryReserve(req, ctx);

  EXPECT_TRUE(first.valid());
  EXPECT_FALSE(second.valid());
  EXPECT_EQ(pool.available(), 0u);
}

TEST(SlotPool, ReserveThrowsWhenEmpty) {
  auto pool = makePool(1);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  (void)pool.reserve(req, ctx);
  EXPECT_THROW(pool.reserve(req, ctx), std::system_error);
}

TEST(SlotPool, TryAcquireReturnsInvalidWhenNoCreated) {
  auto pool = makePool(1);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  auto ref = pool.tryAcquire(req, ctx);
  EXPECT_FALSE(ref.valid());
}

TEST(SlotPool, AcquireThrowsWhenNoCreated) {
  auto pool = makePool(1);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  EXPECT_THROW(pool.acquire(req, ctx), std::system_error);
}

TEST(SlotPool, AcquireReturnsCreatedSlotAfterRelease) {
  auto pool = makePool(1);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  auto reserved = pool.reserve(req, ctx);
  EXPECT_TRUE(pool.emplace(reserved.handle, req, ctx));
  EXPECT_TRUE(pool.release(reserved.handle));

  auto ref = pool.acquire(req, ctx);
  EXPECT_TRUE(ref.valid());
}

TEST(SlotPool, ReleaseReturnsSlotToFreelistAndIncrementsGeneration) {
  auto pool = makePool(1);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  auto first = pool.reserve(req, ctx);
  auto handle = first.handle;
  EXPECT_TRUE(pool.emplace(handle, req, ctx));

  EXPECT_TRUE(pool.release(handle));

  auto second = pool.acquire(req, ctx);
  EXPECT_EQ(second.handle.index, handle.index);
  EXPECT_EQ(static_cast<std::size_t>(second.handle.generation),
            static_cast<std::size_t>(handle.generation) + 1u);
}

TEST(SlotPool, ReleaseRejectsStaleGeneration) {
  auto pool = makePool(1);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  auto first = pool.reserve(req, ctx);
  auto stale = first.handle;
  EXPECT_TRUE(pool.emplace(stale, req, ctx));

  EXPECT_TRUE(pool.release(stale));
  EXPECT_FALSE(pool.release(stale));
}

TEST(SlotPool, EmplaceUsesTraitsCreateAndSetsCreated) {
  auto pool = makePool(1);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  auto slot = pool.reserve(req, ctx);
  EXPECT_TRUE(pool.emplace(slot.handle, req, ctx));
  EXPECT_TRUE(pool.isCreated(slot.handle));
  EXPECT_EQ(pool.get(slot.handle)->value, 42);
}

TEST(SlotPool, EmplaceLambdaOverridesTraitsCreate) {
  auto pool = makePool(1);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  auto slot = pool.reserve(req, ctx);
  EXPECT_TRUE(pool.emplace(slot.handle, req, ctx,
                           [](DummyPayload &payload,
                              const DummyTraits::Request &,
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

  auto slot = pool.reserve(req, ctx);
  EXPECT_TRUE(pool.emplace(slot.handle, req, ctx));
  EXPECT_TRUE(pool.destroy(slot.handle, req, ctx));
  EXPECT_FALSE(pool.isCreated(slot.handle));
  EXPECT_EQ(pool.get(slot.handle)->value, 0);
}

TEST(SlotPool, DestroyLambdaOverridesTraitsDestroy) {
  auto pool = makePool(1);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  auto slot = pool.reserve(req, ctx);
  EXPECT_TRUE(pool.emplace(slot.handle, req, ctx));
  EXPECT_TRUE(pool.destroy(slot.handle, req, ctx,
                           [](DummyPayload &payload,
                              const DummyTraits::Request &,
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

  auto ref = pool.reserve(req, ctx);
  auto stale = ref.handle;
  EXPECT_TRUE(pool.emplace(stale, req, ctx));

  EXPECT_TRUE(pool.release(stale));
  EXPECT_EQ(pool.get(stale), nullptr);
}

TEST(SlotPool, ReleaseDestroysWhenConfigured) {
  auto pool = makeDestroyOnReleasePool(1);
  DestroyOnReleaseTraits::Request req{};
  DestroyOnReleaseTraits::Context ctx{};

  auto slot = pool.reserve(req, ctx);
  EXPECT_TRUE(pool.emplace(slot.handle, req, ctx));
  EXPECT_TRUE(pool.release(slot.handle, req, ctx));
  EXPECT_FALSE(pool.isCreated(slot.handle));
  auto re_reserved = pool.reserve(req, ctx);
  EXPECT_TRUE(re_reserved.valid());
  EXPECT_EQ(re_reserved.payload_ptr->value, -1);
}

TEST(SlotPool, ReleaseFailsWhenNotCreatedInDestroyMode) {
  auto pool = makeDestroyOnReleasePool(1);
  DestroyOnReleaseTraits::Request req{};
  DestroyOnReleaseTraits::Context ctx{};

  auto slot = pool.reserve(req, ctx);
  EXPECT_FALSE(pool.release(slot.handle, req, ctx));
}

TEST(SlotPool, GrowAddsUncreatedSlots) {
  auto pool = makePool(1);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  pool.grow(typename DummyTraits::Config{3});

  EXPECT_EQ(pool.capacity(), 3u);
  EXPECT_EQ(pool.available(), 3u);

  auto ref = pool.reserve(req, ctx);
  EXPECT_TRUE(ref.valid());
  EXPECT_FALSE(pool.isCreated(ref.handle));
}

TEST(SlotPool, GrowAndCreateCreatesNewSlots) {
  auto pool = makePool(1);
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  auto reserved = pool.reserve(req, ctx);
  EXPECT_TRUE(pool.emplace(reserved.handle, req, ctx));
  EXPECT_TRUE(pool.release(reserved.handle));

  EXPECT_TRUE(pool.growAndCreate(typename DummyTraits::Config{3}, req, ctx));
  EXPECT_FALSE(pool.tryReserve(req, ctx).valid());

  auto ref = pool.acquire(req, ctx);
  EXPECT_TRUE(ref.valid());
}

TEST(SlotPool, InitializeAndCreateCreatesAllSlots) {
  Pool pool;
  DummyTraits::Request req{};
  DummyTraits::Context ctx{};

  EXPECT_TRUE(pool.initializeAndCreate(typename DummyTraits::Config{2}, req, ctx));
  EXPECT_EQ(pool.capacity(), 2u);
  EXPECT_EQ(pool.available(), 2u);
  EXPECT_FALSE(pool.tryReserve(req, ctx).valid());
  EXPECT_TRUE(pool.tryAcquire(req, ctx).valid());
}

} // namespace

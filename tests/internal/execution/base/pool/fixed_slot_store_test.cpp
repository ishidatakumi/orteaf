#include "orteaf/internal/execution/base/pool/fixed_slot_store.h"

#include <gtest/gtest.h>
#include <system_error>

#include "orteaf/internal/base/handle.h"

namespace {

struct StoreTag {};
using StoreHandle =
    ::orteaf::internal::base::Handle<StoreTag, std::uint32_t, std::uint8_t>;

struct DummyPayload {
  int value{0};
};

struct DummyTraits {
  using Payload = DummyPayload;
  using Handle = StoreHandle;
  struct Request {
    Handle handle{};
  };
  struct Context {};

  static bool create(Payload &payload, const Request &, const Context &) {
    payload.value = 123;
    return true;
  }

  static void destroy(Payload &payload, const Request &, const Context &) {
    payload.value = 0;
  }
};

struct DestroyOnReleaseTraits {
  using Payload = DummyPayload;
  using Handle = StoreHandle;
  struct Request {
    Handle handle{};
  };
  struct Context {};
  static constexpr bool destroy_on_release = true;

  static bool create(Payload &payload, const Request &, const Context &) {
    payload.value = 55;
    return true;
  }

  static void destroy(Payload &payload, const Request &, const Context &) {
    payload.value = -5;
  }
};

using Store =
    ::orteaf::internal::execution::base::pool::FixedSlotStore<DummyTraits>;
using DestroyOnReleaseStore =
    ::orteaf::internal::execution::base::pool::FixedSlotStore<
        DestroyOnReleaseTraits>;

Store makeStore(std::size_t capacity) {
  Store store;
  store.configure(typename Store::Config{capacity, capacity});
  return store;
}

DestroyOnReleaseStore makeDestroyOnReleaseStore(std::size_t capacity) {
  DestroyOnReleaseStore store;
  store.configure(typename DestroyOnReleaseStore::Config{capacity, capacity});
  return store;
}

TEST(FixedSlotStore, AcquireThrowsWhenNotCreated) {
  auto store = makeStore(1);
  DummyTraits::Context ctx{};
  DummyTraits::Request req{StoreHandle{0, 0}};

  EXPECT_THROW(store.acquireCreated(req, ctx), std::system_error);
}

TEST(FixedSlotStore, TryAcquireInvalidWhenNotCreated) {
  auto store = makeStore(1);
  DummyTraits::Context ctx{};
  DummyTraits::Request req{StoreHandle{0, 0}};

  auto ref = store.tryAcquireCreated(req, ctx);
  EXPECT_FALSE(ref.valid());
}

TEST(FixedSlotStore, EmplaceAndAcquireReturnPayload) {
  auto store = makeStore(1);
  DummyTraits::Context ctx{};
  DummyTraits::Request req{StoreHandle{0, 0}};

  EXPECT_TRUE(store.emplace(req.handle, req, ctx));

  auto ref = store.acquireCreated(req, ctx);
  EXPECT_TRUE(ref.valid());
  EXPECT_EQ(ref.payload_ptr->value, 123);
  EXPECT_EQ(store.get(req.handle)->value, 123);
}

TEST(FixedSlotStore, DestroyClearsCreated) {
  auto store = makeStore(1);
  DummyTraits::Context ctx{};
  DummyTraits::Request req{StoreHandle{0, 0}};

  EXPECT_TRUE(store.emplace(req.handle, req, ctx));
  EXPECT_TRUE(store.destroy(req.handle, req, ctx));
  EXPECT_FALSE(store.isCreated(req.handle));
  EXPECT_EQ(store.get(req.handle), nullptr);
}

TEST(FixedSlotStore, EmplaceLambdaOverridesTraitsCreate) {
  auto store = makeStore(1);
  DummyTraits::Context ctx{};
  DummyTraits::Request req{StoreHandle{0, 0}};

  EXPECT_TRUE(
      store.emplace(req.handle, req, ctx,
                    [](DummyPayload &payload, const DummyTraits::Request &,
                       const DummyTraits::Context &) {
                      payload.value = 7;
                      return true;
                    }));
  EXPECT_EQ(store.get(req.handle)->value, 7);
}

TEST(FixedSlotStore, DestroyLambdaOverridesTraitsDestroy) {
  auto store = makeStore(1);
  DummyTraits::Context ctx{};
  DummyTraits::Request req{StoreHandle{0, 0}};

  EXPECT_TRUE(store.emplace(req.handle, req, ctx));
  EXPECT_TRUE(
      store.destroy(req.handle, req, ctx,
                    [](DummyPayload &payload, const DummyTraits::Request &,
                       const DummyTraits::Context &) {
                      payload.value = -1;
                      return true;
                    }));
  EXPECT_FALSE(store.isCreated(req.handle));
  EXPECT_EQ(store.get(req.handle), nullptr);
}

TEST(FixedSlotStore, ReleaseDestroysWhenConfigured) {
  auto store = makeDestroyOnReleaseStore(1);
  DestroyOnReleaseTraits::Context ctx{};
  DestroyOnReleaseTraits::Request req{StoreHandle{0, 0}};

  EXPECT_TRUE(store.emplace(req.handle, req, ctx));
  EXPECT_TRUE(store.release(req.handle, req, ctx));
  EXPECT_FALSE(store.isCreated(req.handle));
  EXPECT_EQ(store.get(req.handle), nullptr);
}

TEST(FixedSlotStore, ReleaseFailsWhenNotCreatedInDestroyMode) {
  auto store = makeDestroyOnReleaseStore(1);
  DestroyOnReleaseTraits::Context ctx{};
  DestroyOnReleaseTraits::Request req{StoreHandle{0, 0}};

  EXPECT_FALSE(store.release(req.handle, req, ctx));
}

TEST(FixedSlotStore, GrowAddsUncreatedSlots) {
  auto store = makeStore(1);
  DummyTraits::Context ctx{};
  DummyTraits::Request req{StoreHandle{1, 0}};

  store.configure(typename Store::Config{3, 3});

  EXPECT_EQ(store.size(), 3u);
  EXPECT_FALSE(store.isCreated(req.handle));
  EXPECT_EQ(store.get(req.handle), nullptr);
}

TEST(FixedSlotStore, GrowAndCreateCreatesNewSlots) {
  auto store = makeStore(1);
  DummyTraits::Context ctx{};
  DummyTraits::Request req{};

  // First create slot 0
  EXPECT_TRUE(store.emplace(StoreHandle{0, 0}, req, ctx));

  // Then grow and create new slots
  const std::size_t old_capacity =
      store.configure(typename Store::Config{2, 2});
  EXPECT_TRUE(store.createRange(old_capacity, store.size(), req, ctx));

  // Now we have 2 created slots (0 and 1)
  // tryAcquireCreated returns one of the created slots (index 0)
  auto ref = store.tryAcquireCreated(req, ctx);
  EXPECT_TRUE(ref.valid());
  EXPECT_EQ(ref.payload_ptr->value, 123);
}

TEST(FixedSlotStore, ReserveDoesNotChangeSize) {
  Store store;
  store.reserve(5);
  EXPECT_EQ(store.size(), 0u);
  EXPECT_GE(store.capacity(), 5u);
}

TEST(FixedSlotStore, ResizeGrowsSlots) {
  Store store;
  const std::size_t old_size = store.resize(4);
  EXPECT_EQ(old_size, 0u);
  EXPECT_EQ(store.size(), 4u);
}

TEST(FixedSlotStore, InitializeAndCreateCreatesAllSlots) {
  DestroyOnReleaseStore store;
  DestroyOnReleaseTraits::Context ctx{};
  DestroyOnReleaseTraits::Request req{};

  store.configure(typename DestroyOnReleaseStore::Config{2, 2});
  EXPECT_TRUE(store.createAll(req, ctx));

  // tryAcquireCreated returns one of the created slots
  auto ref = store.tryAcquireCreated(req, ctx);
  EXPECT_TRUE(ref.valid());
  EXPECT_EQ(ref.payload_ptr->value, 55);
}

} // namespace

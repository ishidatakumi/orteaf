#include "orteaf/internal/runtime/base/pool/fixed_slot_store.h"

#include <gtest/gtest.h>
#include <system_error>

#include "orteaf/internal/base/handle.h"

namespace {

struct StoreTag {};
using StoreHandle = ::orteaf::internal::base::Handle<StoreTag, std::uint32_t, std::uint8_t>;

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
  struct Config {
    std::size_t capacity{0};
  };

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
  struct Config {
    std::size_t capacity{0};
  };
  static constexpr bool destroy_on_release = true;

  static bool create(Payload &payload, const Request &, const Context &) {
    payload.value = 55;
    return true;
  }

  static void destroy(Payload &payload, const Request &, const Context &) {
    payload.value = -5;
  }
};

using Store = ::orteaf::internal::runtime::base::pool::FixedSlotStore<DummyTraits>;
using DestroyOnReleaseStore =
    ::orteaf::internal::runtime::base::pool::FixedSlotStore<DestroyOnReleaseTraits>;

Store makeStore(std::size_t capacity) {
  Store store;
  store.initialize(typename DummyTraits::Config{capacity});
  return store;
}

DestroyOnReleaseStore makeDestroyOnReleaseStore(std::size_t capacity) {
  DestroyOnReleaseStore store;
  store.initialize(typename DestroyOnReleaseTraits::Config{capacity});
  return store;
}

TEST(FixedSlotStore, AcquireThrowsWhenNotCreated) {
  auto store = makeStore(1);
  DummyTraits::Context ctx{};
  DummyTraits::Request req{StoreHandle{0, 0}};

  EXPECT_THROW(store.acquire(req, ctx), std::system_error);
}

TEST(FixedSlotStore, TryAcquireInvalidWhenNotCreated) {
  auto store = makeStore(1);
  DummyTraits::Context ctx{};
  DummyTraits::Request req{StoreHandle{0, 0}};

  auto ref = store.tryAcquire(req, ctx);
  EXPECT_FALSE(ref.valid());
}

TEST(FixedSlotStore, EmplaceAndAcquireReturnPayload) {
  auto store = makeStore(1);
  DummyTraits::Context ctx{};
  DummyTraits::Request req{StoreHandle{0, 0}};

  EXPECT_TRUE(store.emplace(req.handle, req, ctx));

  auto ref = store.acquire(req, ctx);
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

  EXPECT_TRUE(store.emplace(req.handle, req, ctx,
                            [](DummyPayload &payload,
                               const DummyTraits::Request &,
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
  EXPECT_TRUE(store.destroy(req.handle, req, ctx,
                            [](DummyPayload &payload,
                               const DummyTraits::Request &,
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

  store.grow(typename DummyTraits::Config{3});

  EXPECT_EQ(store.capacity(), 3u);
  EXPECT_FALSE(store.isCreated(req.handle));
  EXPECT_EQ(store.get(req.handle), nullptr);
}

TEST(FixedSlotStore, GrowAndCreateCreatesNewSlots) {
  auto store = makeStore(1);
  DummyTraits::Context ctx{};
  DummyTraits::Request req{StoreHandle{0, 0}};

  EXPECT_TRUE(store.growAndCreate(typename DummyTraits::Config{2}, req, ctx));

  DummyTraits::Request req_new{StoreHandle{1, 0}};
  auto ref = store.acquire(req_new, ctx);
  EXPECT_TRUE(ref.valid());
  EXPECT_EQ(ref.payload_ptr->value, 123);
}

TEST(FixedSlotStore, InitializeAndCreateCreatesAllSlots) {
  DestroyOnReleaseStore store;
  DestroyOnReleaseTraits::Context ctx{};
  DestroyOnReleaseTraits::Request req{StoreHandle{0, 0}};

  EXPECT_TRUE(store.initializeAndCreate(typename DestroyOnReleaseTraits::Config{2},
                                        req, ctx));
  DestroyOnReleaseTraits::Request req_new{StoreHandle{1, 0}};
  auto ref = store.acquire(req_new, ctx);
  EXPECT_TRUE(ref.valid());
  EXPECT_EQ(ref.payload_ptr->value, 55);
}

} // namespace

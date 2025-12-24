#include "orteaf/internal/runtime/base/lease/control_block/strong.h"

#include <gtest/gtest.h>

#include "orteaf/internal/base/handle.h"

namespace {

struct PayloadTag {};
using PayloadHandle = ::orteaf::internal::base::Handle<PayloadTag, std::uint32_t, std::uint8_t>;

struct DummyPayload {
  int value{0};
};

struct DummyPool {
  int marker{0};
  std::size_t release_calls{0};
  PayloadHandle last_handle{PayloadHandle::invalid()};

  bool release(PayloadHandle handle) {
    ++release_calls;
    last_handle = handle;
    return true;
  }
};

using StrongCB = ::orteaf::internal::runtime::base::StrongControlBlock<
    PayloadHandle, DummyPayload, DummyPool>;

TEST(StrongControlBlock, BindPayloadStoresHandlePtrAndPool) {
  DummyPool pool{};
  DummyPayload payload{};
  StrongCB cb;
  const PayloadHandle handle{1, 2};

  EXPECT_TRUE(cb.tryBindPayload(handle, &payload, &pool));

  EXPECT_TRUE(cb.hasPayload());
  EXPECT_EQ(cb.payloadHandle(), handle);
  EXPECT_EQ(cb.payloadPtr(), &payload);
  EXPECT_EQ(cb.payloadPool(), &pool);
}

TEST(StrongControlBlock, StrongCountIncrementsAndReleaseSignalsLast) {
  StrongCB cb;

  EXPECT_EQ(cb.count(), 0u);
  cb.acquire();
  cb.acquire();
  EXPECT_EQ(cb.count(), 2u);

  EXPECT_FALSE(cb.release());
  EXPECT_TRUE(cb.release());
  EXPECT_EQ(cb.count(), 0u);
}

TEST(StrongControlBlock, ReleaseDoesNotUnderflow) {
  StrongCB cb;

  EXPECT_EQ(cb.count(), 0u);
  EXPECT_FALSE(cb.release());
  EXPECT_EQ(cb.count(), 0u);
}

TEST(StrongControlBlock, ReleaseCallsPoolOnLastStrongRef) {
  DummyPool pool{};
  DummyPayload payload{};
  StrongCB cb;
  const PayloadHandle handle{1, 2};

  EXPECT_TRUE(cb.tryBindPayload(handle, &payload, &pool));
  cb.acquire();
  cb.acquire();

  EXPECT_FALSE(cb.release());
  EXPECT_EQ(pool.release_calls, 0u);

  EXPECT_TRUE(cb.release());
  EXPECT_EQ(pool.release_calls, 1u);
  EXPECT_EQ(pool.last_handle, handle);
  EXPECT_FALSE(cb.hasPayload());
}

TEST(StrongControlBlock, ReleaseSkipsPoolWhenNull) {
  DummyPool pool{};
  DummyPayload payload{};
  StrongCB cb;
  const PayloadHandle handle{1, 2};

  EXPECT_TRUE(cb.tryBindPayload(handle, &payload, nullptr));
  cb.acquire();

  EXPECT_TRUE(cb.release());
  EXPECT_EQ(pool.release_calls, 0u);
}

TEST(StrongControlBlock, TryBindPayloadFailsWhenReferencesRemain) {
  DummyPool pool{};
  DummyPayload payload{};
  StrongCB cb;
  const PayloadHandle handle{5, 6};

  cb.acquire();
  EXPECT_FALSE(cb.tryBindPayload(handle, &payload, &pool));
  EXPECT_FALSE(cb.hasPayload());

  EXPECT_TRUE(cb.release());
  EXPECT_TRUE(cb.tryBindPayload(handle, &payload, &pool));
  EXPECT_TRUE(cb.hasPayload());
}

TEST(StrongControlBlock, TryBindPayloadFailsWhenAlreadyBound) {
  DummyPool pool{};
  DummyPayload payload{};
  StrongCB cb;
  const PayloadHandle handle{3, 4};

  EXPECT_TRUE(cb.tryBindPayload(handle, &payload, &pool));
  EXPECT_FALSE(cb.tryBindPayload(handle, &payload, &pool));
}

} // namespace

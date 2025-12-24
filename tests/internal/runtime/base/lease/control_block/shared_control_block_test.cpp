#include "orteaf/internal/runtime/base/lease/control_block/shared.h"

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

using SharedCB = ::orteaf::internal::runtime::base::SharedControlBlock<
    PayloadHandle, DummyPayload, DummyPool>;

TEST(SharedControlBlock, BindPayloadStoresHandlePtrAndPool) {
  DummyPool pool{};
  DummyPayload payload{};
  SharedCB cb;
  const PayloadHandle handle{1, 2};

  EXPECT_TRUE(cb.tryBindPayload(handle, &payload, &pool));

  EXPECT_TRUE(cb.hasPayload());
  EXPECT_EQ(cb.payloadHandle(), handle);
  EXPECT_EQ(cb.payloadPtr(), &payload);
  EXPECT_EQ(cb.payloadPool(), &pool);
}

TEST(SharedControlBlock, StrongAndWeakCountsBehave) {
  SharedCB cb;

  cb.acquire();
  cb.acquire();
  cb.acquireWeak();
  cb.acquireWeak();

  EXPECT_EQ(cb.count(), 2u);
  EXPECT_EQ(cb.weakCount(), 2u);

  EXPECT_FALSE(cb.release());
  EXPECT_TRUE(cb.release());
  EXPECT_FALSE(cb.releaseWeak());
  EXPECT_TRUE(cb.releaseWeak());

  EXPECT_EQ(cb.count(), 0u);
  EXPECT_EQ(cb.weakCount(), 0u);
}

TEST(SharedControlBlock, ReleaseWeakDoesNotUnderflow) {
  SharedCB cb;

  EXPECT_EQ(cb.weakCount(), 0u);
  EXPECT_FALSE(cb.releaseWeak());
  EXPECT_EQ(cb.weakCount(), 0u);
}

TEST(SharedControlBlock, ReleaseCallsPoolOnLastStrongRef) {
  DummyPool pool{};
  DummyPayload payload{};
  SharedCB cb;
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

TEST(SharedControlBlock, ReleaseDoesNotUnderflow) {
  SharedCB cb;

  EXPECT_EQ(cb.count(), 0u);
  EXPECT_FALSE(cb.release());
  EXPECT_EQ(cb.count(), 0u);
}

TEST(SharedControlBlock, ReleaseSkipsPoolWhenNull) {
  DummyPool pool{};
  DummyPayload payload{};
  SharedCB cb;
  const PayloadHandle handle{1, 2};

  EXPECT_TRUE(cb.tryBindPayload(handle, &payload, nullptr));
  cb.acquire();

  EXPECT_TRUE(cb.release());
  EXPECT_EQ(pool.release_calls, 0u);
}

TEST(SharedControlBlock, TryPromoteDependsOnStrongCount) {
  SharedCB cb;

  EXPECT_FALSE(cb.tryPromote());

  cb.acquire();
  EXPECT_TRUE(cb.tryPromote());
  EXPECT_EQ(cb.count(), 2u);
}

TEST(SharedControlBlock, TryBindPayloadFailsWhenReferencesRemain) {
  DummyPool pool{};
  DummyPayload payload{};
  SharedCB cb;
  const PayloadHandle handle{7, 8};

  cb.acquire();
  cb.acquireWeak();
  EXPECT_FALSE(cb.tryBindPayload(handle, &payload, &pool));
  EXPECT_FALSE(cb.hasPayload());

  EXPECT_TRUE(cb.release());
  EXPECT_TRUE(cb.releaseWeak());
  EXPECT_TRUE(cb.tryBindPayload(handle, &payload, &pool));
  EXPECT_TRUE(cb.hasPayload());
}

TEST(SharedControlBlock, TryBindPayloadFailsWhenAlreadyBound) {
  DummyPool pool{};
  DummyPayload payload{};
  SharedCB cb;
  const PayloadHandle handle{3, 4};

  EXPECT_TRUE(cb.tryBindPayload(handle, &payload, &pool));
  EXPECT_FALSE(cb.tryBindPayload(handle, &payload, &pool));
}

} // namespace

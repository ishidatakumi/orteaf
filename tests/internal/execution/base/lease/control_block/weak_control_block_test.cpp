#include "orteaf/internal/execution/base/lease/control_block/weak.h"

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
};

using WeakCB = ::orteaf::internal::execution::base::WeakControlBlock<
    PayloadHandle, DummyPayload, DummyPool>;

TEST(WeakControlBlock, BindPayloadStoresHandlePtrAndPool) {
  DummyPool pool{};
  DummyPayload payload{};
  WeakCB cb;
  const PayloadHandle handle{1, 2};

  EXPECT_TRUE(cb.tryBindPayload(handle, &payload, &pool));

  EXPECT_TRUE(cb.hasPayload());
  EXPECT_EQ(cb.payloadHandle(), handle);
  EXPECT_EQ(cb.payloadPtr(), &payload);
  EXPECT_EQ(cb.payloadPool(), &pool);
}

TEST(WeakControlBlock, WeakCountIncrementsAndDecrements) {
  WeakCB cb;

  EXPECT_EQ(cb.weakCount(), 0u);
  cb.acquireWeak();
  cb.acquireWeak();
  EXPECT_EQ(cb.weakCount(), 2u);

  EXPECT_FALSE(cb.releaseWeak());
  EXPECT_TRUE(cb.releaseWeak());
  EXPECT_EQ(cb.weakCount(), 0u);
}

TEST(WeakControlBlock, ReleaseWeakDoesNotUnderflow) {
  WeakCB cb;

  EXPECT_EQ(cb.weakCount(), 0u);
  EXPECT_FALSE(cb.releaseWeak());
  EXPECT_EQ(cb.weakCount(), 0u);
}

TEST(WeakControlBlock, TryBindPayloadFailsWhenWeakReferencesRemain) {
  DummyPool pool{};
  DummyPayload payload{};
  WeakCB cb;
  const PayloadHandle handle{1, 2};

  cb.acquireWeak();
  EXPECT_FALSE(cb.tryBindPayload(handle, &payload, &pool));
  EXPECT_FALSE(cb.hasPayload());

  EXPECT_TRUE(cb.releaseWeak());
  EXPECT_TRUE(cb.tryBindPayload(handle, &payload, &pool));
  EXPECT_TRUE(cb.hasPayload());
}

TEST(WeakControlBlock, TryBindPayloadFailsWhenAlreadyBound) {
  DummyPool pool{};
  DummyPayload payload{};
  WeakCB cb;
  const PayloadHandle handle{3, 4};

  EXPECT_TRUE(cb.tryBindPayload(handle, &payload, &pool));
  EXPECT_FALSE(cb.tryBindPayload(handle, &payload, &pool));
}

} // namespace

#include "orteaf/internal/execution/base/lease/strong_lease.h"
#include "orteaf/internal/execution/base/lease/weak_lease.h"

#include <cstddef>

#include <gtest/gtest.h>

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/execution/base/lease/control_block/shared.h"

namespace {

struct LeaseTag {};
using ControlBlockHandle =
    ::orteaf::internal::base::Handle<LeaseTag, std::uint32_t, std::uint8_t>;

struct DummyPool {
  bool release(ControlBlockHandle handle) {
    last_handle = handle;
    ++release_calls;
    return true;
  }

  ControlBlockHandle last_handle{ControlBlockHandle::invalid()};
  std::size_t release_calls{0};
};

using ControlBlock = ::orteaf::internal::execution::base::SharedControlBlock<
    ControlBlockHandle, int, DummyPool>;

struct DummyManager {
  using StrongLease = ::orteaf::internal::execution::base::StrongLease<
      ControlBlockHandle, ControlBlock, DummyPool, DummyManager>;
  using WeakLease = ::orteaf::internal::execution::base::WeakLease<
      ControlBlockHandle, ControlBlock, DummyPool, DummyManager>;

  static StrongLease makeStrong(ControlBlock *control_block, DummyPool *pool,
                                ControlBlockHandle handle) {
    return StrongLease(control_block, pool, handle);
  }

  static WeakLease makeWeak(ControlBlock *control_block, DummyPool *pool,
                            ControlBlockHandle handle) {
    return WeakLease(control_block, pool, handle);
  }
};

TEST(StrongLease, CopyIncrementsStrongCount) {
  ControlBlock control_block;
  DummyPool pool;
  ControlBlockHandle handle{0, 0};

  auto lease = DummyManager::makeStrong(&control_block, &pool, handle);
  EXPECT_EQ(control_block.count(), 1u);

  auto copy = lease;
  EXPECT_EQ(control_block.count(), 2u);
}

TEST(WeakLease, CopyIncrementsWeakCount) {
  ControlBlock control_block;
  DummyPool pool;
  ControlBlockHandle handle{0, 0};

  auto lease = DummyManager::makeWeak(&control_block, &pool, handle);
  EXPECT_EQ(control_block.weakCount(), 1u);

  auto copy = lease;
  EXPECT_EQ(control_block.weakCount(), 2u);
}

TEST(WeakLease, LockPromotesToStrongLease) {
  ControlBlock control_block;
  DummyPool pool;
  ControlBlockHandle handle{0, 0};

  auto strong = DummyManager::makeStrong(&control_block, &pool, handle);
  auto weak = DummyManager::makeWeak(&control_block, &pool, handle);

  EXPECT_EQ(control_block.count(), 1u);

  auto promoted = weak.lock();
  EXPECT_TRUE(promoted);
  EXPECT_EQ(control_block.count(), 2u);
}

TEST(WeakLease, LockFailsWhenNoStrongRefs) {
  ControlBlock control_block;
  DummyPool pool;
  ControlBlockHandle handle{0, 0};

  auto weak = DummyManager::makeWeak(&control_block, &pool, handle);
  EXPECT_EQ(control_block.count(), 0u);

  auto promoted = weak.lock();
  EXPECT_FALSE(promoted);
}

TEST(StrongWeakLease, ReturnControlBlockToPoolWhenCountsZero) {
  ControlBlock control_block;
  DummyPool pool;
  ControlBlockHandle handle{0, 0};

  {
    auto strong = DummyManager::makeStrong(&control_block, &pool, handle);
    auto weak = DummyManager::makeWeak(&control_block, &pool, handle);
    EXPECT_EQ(control_block.count(), 1u);
    EXPECT_EQ(control_block.weakCount(), 1u);
  }

  EXPECT_EQ(pool.release_calls, 1u);
  EXPECT_TRUE(pool.last_handle.isValid());
}

TEST(WeakLease, ConstructFromStrongLease) {
  ControlBlock control_block;
  DummyPool pool;
  ControlBlockHandle handle{0, 0};

  auto strong = DummyManager::makeStrong(&control_block, &pool, handle);
  EXPECT_EQ(control_block.count(), 1u);
  EXPECT_EQ(control_block.weakCount(), 0u);

  DummyManager::WeakLease weak(strong);
  EXPECT_EQ(control_block.count(), 1u);
  EXPECT_EQ(control_block.weakCount(), 1u);
  EXPECT_TRUE(weak);
}

TEST(WeakLease, ConstructFromInvalidStrongLease) {
  DummyManager::StrongLease strong;
  EXPECT_FALSE(strong);

  DummyManager::WeakLease weak(strong);
  EXPECT_FALSE(weak);
}

TEST(WeakLease, ConstructedFromStrongCanPromote) {
  ControlBlock control_block;
  DummyPool pool;
  ControlBlockHandle handle{0, 0};

  auto strong = DummyManager::makeStrong(&control_block, &pool, handle);
  DummyManager::WeakLease weak(strong);

  auto promoted = weak.lock();
  EXPECT_TRUE(promoted);
  EXPECT_EQ(control_block.count(), 2u);
}

} // namespace

#pragma once

#include <memory>
#include <type_traits>

#include <gmock/gmock.h>

#include <orteaf/internal/runtime/mps/platform/mps_slow_ops.h>
#include <tests/internal/runtime/mps/manager/testing/backend_mock.h>

namespace orteaf::tests::runtime::mps::testing {

template <class BackendOpsT, bool IsMockV> struct BackendOpsProvider {
  using SlowOps = BackendOpsT;
  static constexpr bool is_mock = IsMockV;

  struct Context {
    Context() = default;
  };

  static void setUp(Context &) {}
  static void tearDown(Context &) {}
};

// Real provider uses MpsSlowOpsImpl
struct RealBackendOpsProvider {
  using SlowOps = ::orteaf::internal::runtime::mps::platform::MpsSlowOpsImpl;
  static constexpr bool is_mock = false;

  struct Context {
    SlowOps ops;
  };

  static void setUp(Context &) {}
  static void tearDown(Context &) {}

  static SlowOps *getOps(Context &ctx) { return &ctx.ops; }
};

// Mock provider uses MpsBackendOpsMock
struct MockBackendOpsProvider {
  using SlowOps = ::orteaf::tests::runtime::mps::MpsBackendOpsMock;
  static constexpr bool is_mock = true;

  struct Context {
    ::testing::NiceMock<SlowOps> mock;
  };

  static void setUp(Context &) {}
  static void tearDown(Context &) {}

  static SlowOps *getOps(Context &ctx) { return &ctx.mock; }
  static SlowOps &mock(Context &ctx) { return ctx.mock; }
};

} // namespace orteaf::tests::runtime::mps::testing

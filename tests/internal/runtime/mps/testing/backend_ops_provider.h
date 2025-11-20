#pragma once

#include <memory>
#include <type_traits>

#include <gmock/gmock.h>

#include "orteaf/internal/runtime/backend_ops/mps/mps_slow_ops.h"
#include "tests/internal/runtime/mps/testing/backend_mock.h"

namespace orteaf::tests::runtime::mps::testing {

template <class BackendOpsT, bool IsMockV> struct BackendOpsProvider {
  using BackendOps = BackendOpsT;
  static constexpr bool is_mock = IsMockV;

  struct Context {
    Context() = default;
  };

  static void setUp(Context &) {}
  static void tearDown(Context &) {}
};

// Real provider uses MpsSlowOpsImpl
struct RealBackendOpsProvider {
  using BackendOps =
      ::orteaf::internal::runtime::backend_ops::mps::MpsSlowOpsImpl;
  static constexpr bool is_mock = false;

  struct Context {
    BackendOps ops;
  };

  static void setUp(Context &) {}
  static void tearDown(Context &) {}

  static BackendOps *getOps(Context &ctx) { return &ctx.ops; }
};

// Mock provider uses MpsBackendOpsMock
struct MockBackendOpsProvider {
  using BackendOps = ::orteaf::tests::runtime::mps::MpsBackendOpsMock;
  static constexpr bool is_mock = true;

  struct Context {
    ::testing::NiceMock<BackendOps> mock;
  };

  static void setUp(Context &) {}
  static void tearDown(Context &) {}

  static BackendOps *getOps(Context &ctx) { return &ctx.mock; }
  static BackendOps &mock(Context &ctx) { return ctx.mock; }
};

} // namespace orteaf::tests::runtime::mps::testing

#pragma once

#include <cstddef>

#include <gtest/gtest.h>

#include <tests/internal/execution/mps/manager/testing/backend_ops_provider.h>
#include <tests/internal/execution/mps/manager/testing/manager_adapter.h>

namespace orteaf::tests::execution::mps::testing {

template <class Provider, class ManagerType>
class RuntimeManagerFixture : public ::testing::Test {
protected:
  using SlowOps = typename Provider::SlowOps;
  using Manager = ManagerType;
  using Adapter = ManagerAdapter<Manager, Provider>;
  using Context = typename Provider::Context;

  // static_assert(::orteaf::internal::execution::mps::platform::MpsRuntimeBackendOps<SlowOps>);

  void SetUp() override {
    adapter_.bind(manager_, context_);
    onPreManagerSetUp();
    Provider::setUp(context_);
    onPostManagerSetUp();
  }

  void TearDown() override {
    onPreManagerTearDown();
    Provider::tearDown(context_);
    onPostManagerTearDown();
  }

  virtual void onPreManagerSetUp() {}
  virtual void onPostManagerSetUp() {}
  virtual void onPreManagerTearDown() {}
  virtual void onPostManagerTearDown() {}

  Manager &manager() { return adapter_.manager(); }
  const Manager &manager() const { return adapter_.manager(); }

  Adapter &adapter() { return adapter_; }
  const Adapter &adapter() const { return adapter_; }

  Context &context() { return context_; }
  const Context &context() const { return context_; }

  auto *getOps() { return Provider::getOps(context_); }

  Context context_{};
  Manager manager_{};
  Adapter adapter_{};
};

} // namespace orteaf::tests::execution::mps::testing

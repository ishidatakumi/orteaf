#pragma once

#include <cstddef>

#include <gtest/gtest.h>

#include "orteaf/internal/runtime/backend_ops/mps/mps_backend_ops_concepts.h"
#include "tests/internal/runtime/mps/testing/backend_ops_provider.h"
#include "tests/internal/runtime/mps/testing/manager_adapter.h"

namespace orteaf::tests::runtime::mps::testing {

template <class Provider, template <class> class ManagerTemplate>
class RuntimeManagerFixture : public ::testing::Test {
protected:
    using BackendOps = typename Provider::BackendOps;
    using Manager = ManagerTemplate<BackendOps>;
    using Adapter = ManagerAdapter<Manager, Provider>;
    using Context = typename Provider::Context;

    static_assert(::orteaf::internal::runtime::backend_ops::mps::MpsRuntimeBackendOps<BackendOps>);

    void SetUp() override {
        manager_ = Manager{};
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

    Manager& manager() { return adapter_.manager(); }
    const Manager& manager() const { return adapter_.manager(); }

    Adapter& adapter() { return adapter_; }
    const Adapter& adapter() const { return adapter_; }

    Context& context() { return context_; }
    const Context& context() const { return context_; }

    Context context_{};
    Manager manager_{};
    Adapter adapter_{};
};

}  // namespace orteaf::tests::runtime::mps::testing

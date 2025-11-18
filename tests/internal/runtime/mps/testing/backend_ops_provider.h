#pragma once

#include <type_traits>

#include <gmock/gmock.h>

#include "orteaf/internal/runtime/backend_ops/mps/mps_backend_ops.h"
#include "orteaf/internal/runtime/backend_ops/mps/mps_backend_ops_concepts.h"
#include "tests/internal/runtime/mps/testing/backend_mock.h"

namespace orteaf::tests::runtime::mps::testing {

template <class BackendOpsT, bool IsMockV>
struct BackendOpsProvider {
    using BackendOps = BackendOpsT;
    static constexpr bool is_mock = IsMockV;
    static_assert(::orteaf::internal::runtime::backend_ops::mps::MpsRuntimeBackendOps<BackendOps>);

    struct Context {
        Context() = default;
    };

    static void setUp(Context&) {}
    static void tearDown(Context&) {}
};

using RealBackendOpsProvider = BackendOpsProvider<
    ::orteaf::internal::runtime::backend_ops::mps::MpsBackendOps,
    false>;

struct MockBackendOpsProvider
    : BackendOpsProvider<::orteaf::tests::runtime::mps::MpsBackendOpsMockAdapter, true> {
    using Mock = ::orteaf::tests::runtime::mps::MpsBackendOpsMock;
    using Guard = ::orteaf::tests::runtime::mps::MpsBackendOpsMockRegistry::Guard;

    struct Context {
        ::testing::NiceMock<Mock> mock;
        Guard guard;

        Context() : mock{}, guard(mock) {}
    };

    static void setUp(Context&) {}
    static void tearDown(Context&) {}

    static Mock& mock(Context& ctx) { return ctx.mock; }
};

}  // namespace orteaf::tests::runtime::mps::testing

#include <gtest/gtest.h>

#include <string_view>

#include <orteaf/internal/backend/backend.h>

namespace backend = orteaf::internal::backend;

TEST(BackendTablesTest, BasicEnumerationProperties) {
    EXPECT_EQ(backend::kBackendCount, backend::allBackends().size());
    EXPECT_TRUE(backend::isValidIndex(0));
    EXPECT_FALSE(backend::isValidIndex(backend::kBackendCount));

    EXPECT_EQ(backend::fromIndex(0), backend::allBackends().front());
    EXPECT_EQ(backend::idOf(backend::fromIndex(0)), std::string_view("cuda"));
}

TEST(BackendTablesTest, MetadataMatchesCatalog) {
    constexpr auto cuda = backend::Backend::cuda;
    EXPECT_EQ(backend::displayNameOf(cuda), "CUDA");
    EXPECT_EQ(backend::modulePathOf(cuda), "@orteaf/internal/backend/cuda");
    EXPECT_EQ(backend::descriptionOf(cuda), "NVIDIA CUDA 実装");

    constexpr auto mps = backend::Backend::mps;
    EXPECT_EQ(backend::displayNameOf(mps), "MPS");
    EXPECT_EQ(backend::modulePathOf(mps), "@orteaf/internal/backend/mps");
    EXPECT_EQ(backend::descriptionOf(mps), "macOS/iOS 向け Metal Performance Shaders 実装");

    constexpr auto cpu = backend::Backend::cpu;
    EXPECT_EQ(backend::displayNameOf(cpu), "CPU");
    EXPECT_EQ(backend::modulePathOf(cpu), "@orteaf/internal/backend/cpu");
    EXPECT_EQ(backend::descriptionOf(cpu), "汎用 CPU 実装");
}

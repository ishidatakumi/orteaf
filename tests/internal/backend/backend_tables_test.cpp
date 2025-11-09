#include <gtest/gtest.h>

#include <string_view>

#include <orteaf/internal/backend/backend.h>

namespace backend = orteaf::internal::backend;

TEST(BackendTablesTest, BasicEnumerationProperties) {
    EXPECT_EQ(backend::kBackendCount, backend::AllBackends().size());
    EXPECT_TRUE(backend::IsValidIndex(0));
    EXPECT_FALSE(backend::IsValidIndex(backend::kBackendCount));

    EXPECT_EQ(backend::FromIndex(0), backend::AllBackends().front());
    EXPECT_EQ(backend::IdOf(backend::FromIndex(0)), std::string_view("cuda"));
}

TEST(BackendTablesTest, MetadataMatchesCatalog) {
    constexpr auto cuda = backend::Backend::cuda;
    EXPECT_EQ(backend::DisplayNameOf(cuda), "CUDA");
    EXPECT_EQ(backend::ModulePathOf(cuda), "@orteaf/internal/backend/cuda");
    EXPECT_EQ(backend::DescriptionOf(cuda), "NVIDIA CUDA 実装");

    constexpr auto mps = backend::Backend::mps;
    EXPECT_EQ(backend::DisplayNameOf(mps), "MPS");
    EXPECT_EQ(backend::ModulePathOf(mps), "@orteaf/internal/backend/mps");
    EXPECT_EQ(backend::DescriptionOf(mps), "macOS/iOS 向け Metal Performance Shaders 実装");

    constexpr auto cpu = backend::Backend::cpu;
    EXPECT_EQ(backend::DisplayNameOf(cpu), "CPU");
    EXPECT_EQ(backend::ModulePathOf(cpu), "@orteaf/internal/backend/cpu");
    EXPECT_EQ(backend::DescriptionOf(cpu), "汎用 CPU 実装");
}

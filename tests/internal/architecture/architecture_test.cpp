#include "orteaf/internal/architecture/architecture.h"

#include <gtest/gtest.h>

namespace arch = orteaf::internal::architecture;
namespace backend = orteaf::internal::backend;

TEST(ArchitectureBasic, GenericLocalIndexIsZero) {
    EXPECT_EQ(arch::LocalIndexOf(arch::Architecture::cuda_generic), 0);
    EXPECT_EQ(arch::LocalIndexOf(arch::Architecture::mps_generic), 0);
    EXPECT_EQ(arch::LocalIndexOf(arch::Architecture::cpu_generic), 0);

    EXPECT_TRUE(arch::IsGeneric(arch::Architecture::cuda_generic));
    EXPECT_TRUE(arch::IsGeneric(arch::Architecture::mps_generic));
    EXPECT_TRUE(arch::IsGeneric(arch::Architecture::cpu_generic));
}

TEST(ArchitectureBasic, LocalIndicesIncrementPerBackend) {
    EXPECT_EQ(arch::LocalIndexOf(arch::Architecture::cuda_sm80), 1);
    EXPECT_EQ(arch::LocalIndexOf(arch::Architecture::cuda_sm90), 2);
    EXPECT_EQ(arch::LocalIndexOf(arch::Architecture::mps_m2), 1);
    EXPECT_EQ(arch::LocalIndexOf(arch::Architecture::mps_m3), 2);
}

TEST(ArchitectureMetadata, BackendAssociationMatches) {
    EXPECT_EQ(arch::BackendOf(arch::Architecture::cuda_sm80), backend::Backend::cuda);
    EXPECT_EQ(arch::BackendOf(arch::Architecture::mps_m3), backend::Backend::mps);
    EXPECT_EQ(arch::BackendOf(arch::Architecture::cpu_zen4), backend::Backend::cpu);
}

TEST(ArchitectureMetadata, IdAndDisplayNameMatchYaml) {
    EXPECT_EQ(arch::IdOf(arch::Architecture::cuda_sm80), std::string_view("sm80"));
    EXPECT_EQ(arch::DisplayNameOf(arch::Architecture::cuda_sm80), std::string_view("CUDA SM80"));
    EXPECT_EQ(arch::DescriptionOf(arch::Architecture::cuda_sm80),
              std::string_view("Ampere 世代 GPU (A100 など) 向け最適化"));

    EXPECT_EQ(arch::IdOf(arch::Architecture::cpu_skylake), std::string_view("skylake"));
    EXPECT_EQ(arch::DisplayNameOf(arch::Architecture::cpu_skylake), std::string_view("Skylake AVX512"));
}

TEST(ArchitectureLookup, BackendCountsIncludeGeneric) {
    EXPECT_EQ(arch::CountForBackend(backend::Backend::cuda), 3u);
    EXPECT_EQ(arch::CountForBackend(backend::Backend::mps), 3u);
    EXPECT_EQ(arch::CountForBackend(backend::Backend::cpu), 3u);
}

TEST(ArchitectureLookup, ArchitecturesOfReturnsContiguousSpan) {
    const auto cuda_archs = arch::ArchitecturesOf(backend::Backend::cuda);
    ASSERT_EQ(cuda_archs.size(), 3u);
    EXPECT_EQ(cuda_archs.front(), arch::Architecture::cuda_generic);
    EXPECT_EQ(cuda_archs.back(), arch::Architecture::cuda_sm90);

    const auto cpu_archs = arch::ArchitecturesOf(backend::Backend::cpu);
    ASSERT_EQ(cpu_archs.size(), 3u);
    EXPECT_EQ(cpu_archs[1], arch::Architecture::cpu_zen4);
}

TEST(ArchitectureLookup, FromBackendAndLocalIndexRoundsTrip) {
    const auto arch_id = arch::FromBackendAndLocalIndex(backend::Backend::cuda, 2);
    EXPECT_EQ(arch_id, arch::Architecture::cuda_sm90);
    EXPECT_TRUE(arch::HasLocalIndex(backend::Backend::cuda, 2));
    EXPECT_FALSE(arch::HasLocalIndex(backend::Backend::cuda, 5));
}

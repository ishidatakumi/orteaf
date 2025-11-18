#include "orteaf/internal/architecture/architecture.h"

#include <algorithm>

#include <gtest/gtest.h>

namespace arch = orteaf::internal::architecture;
namespace backend = orteaf::internal::backend;

TEST(ArchitectureBasic, GenericLocalIndexIsZero) {
    EXPECT_EQ(arch::localIndexOf(arch::Architecture::cuda_generic), 0);
    EXPECT_EQ(arch::localIndexOf(arch::Architecture::mps_generic), 0);
    EXPECT_EQ(arch::localIndexOf(arch::Architecture::cpu_generic), 0);

    EXPECT_TRUE(arch::isGeneric(arch::Architecture::cuda_generic));
    EXPECT_TRUE(arch::isGeneric(arch::Architecture::mps_generic));
    EXPECT_TRUE(arch::isGeneric(arch::Architecture::cpu_generic));
}

TEST(ArchitectureBasic, LocalIndicesIncrementPerBackend) {
    EXPECT_EQ(arch::localIndexOf(arch::Architecture::cuda_sm80), 1);
    EXPECT_EQ(arch::localIndexOf(arch::Architecture::cuda_sm86), 2);
    EXPECT_EQ(arch::localIndexOf(arch::Architecture::cuda_sm90), 3);
    EXPECT_EQ(arch::localIndexOf(arch::Architecture::mps_m2), 1);
    EXPECT_EQ(arch::localIndexOf(arch::Architecture::mps_m3), 2);
    EXPECT_EQ(arch::localIndexOf(arch::Architecture::mps_m4), 3);
}

TEST(ArchitectureMetadata, BackendAssociationMatches) {
    EXPECT_EQ(arch::backendOf(arch::Architecture::cuda_sm80), backend::Backend::cuda);
    EXPECT_EQ(arch::backendOf(arch::Architecture::mps_m3), backend::Backend::mps);
    EXPECT_EQ(arch::backendOf(arch::Architecture::cpu_zen4), backend::Backend::cpu);
}

TEST(ArchitectureMetadata, IdAndDisplayNameMatchYaml) {
    EXPECT_EQ(arch::idOf(arch::Architecture::cuda_sm80), std::string_view("sm80"));
    EXPECT_EQ(arch::displayNameOf(arch::Architecture::cuda_sm80), std::string_view("CUDA SM80"));
    EXPECT_EQ(arch::descriptionOf(arch::Architecture::cuda_sm80),
              std::string_view("Ampere 世代 GPU (A100 など) 向け最適化"));

    EXPECT_EQ(arch::idOf(arch::Architecture::cpu_skylake), std::string_view("skylake"));
    EXPECT_EQ(arch::displayNameOf(arch::Architecture::cpu_skylake), std::string_view("Skylake AVX512"));
}

TEST(ArchitectureLookup, BackendCountsIncludeGeneric) {
    auto verify_backend = [](backend::Backend backend_id) {
        const auto count = arch::countForBackend(backend_id);
        const auto span = arch::architecturesOf(backend_id);
        EXPECT_EQ(count, span.size());
        ASSERT_GE(count, 1u);
        EXPECT_TRUE(arch::isGeneric(span.front()));
    };
    verify_backend(backend::Backend::cuda);
    verify_backend(backend::Backend::mps);
    verify_backend(backend::Backend::cpu);
}

TEST(ArchitectureLookup, ArchitecturesOfReturnsContiguousSpan) {
    const auto cuda_archs = arch::architecturesOf(backend::Backend::cuda);
    ASSERT_GE(cuda_archs.size(), 4u);
    EXPECT_EQ(cuda_archs.front(), arch::Architecture::cuda_generic);
    EXPECT_NE(std::find(cuda_archs.begin(), cuda_archs.end(), arch::Architecture::cuda_sm80), cuda_archs.end());
    EXPECT_EQ(cuda_archs.back(), arch::Architecture::cuda_sm90);

    const auto cpu_archs = arch::architecturesOf(backend::Backend::cpu);
    ASSERT_GE(cpu_archs.size(), 3u);
    EXPECT_NE(std::find(cpu_archs.begin(), cpu_archs.end(), arch::Architecture::cpu_zen4), cpu_archs.end());
    EXPECT_NE(std::find(cpu_archs.begin(), cpu_archs.end(), arch::Architecture::cpu_intel_comet_lake), cpu_archs.end());
}

TEST(ArchitectureLookup, FromBackendAndLocalIndexRoundsTrip) {
    const auto arch_id = arch::fromBackendAndLocalIndex(backend::Backend::cuda, 3);
    EXPECT_EQ(arch_id, arch::Architecture::cuda_sm90);
    EXPECT_TRUE(arch::hasLocalIndex(backend::Backend::cuda, 3));
    EXPECT_FALSE(arch::hasLocalIndex(backend::Backend::cuda, 5));
}

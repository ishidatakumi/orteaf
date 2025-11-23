#include "orteaf/internal/architecture/architecture.h"

#include <algorithm>

#include <gtest/gtest.h>

namespace arch = orteaf::internal::architecture;
namespace backend = orteaf::internal::backend;

TEST(ArchitectureBasic, GenericLocalIndexIsZero) {
    EXPECT_EQ(arch::localIndexOf(arch::Architecture::CudaGeneric), 0);
    EXPECT_EQ(arch::localIndexOf(arch::Architecture::MpsGeneric), 0);
    EXPECT_EQ(arch::localIndexOf(arch::Architecture::CpuGeneric), 0);

    EXPECT_TRUE(arch::isGeneric(arch::Architecture::CudaGeneric));
    EXPECT_TRUE(arch::isGeneric(arch::Architecture::MpsGeneric));
    EXPECT_TRUE(arch::isGeneric(arch::Architecture::CpuGeneric));
}

TEST(ArchitectureBasic, LocalIndicesIncrementPerBackend) {
    EXPECT_EQ(arch::localIndexOf(arch::Architecture::CudaSm80), 1);
    EXPECT_EQ(arch::localIndexOf(arch::Architecture::CudaSm86), 2);
    EXPECT_EQ(arch::localIndexOf(arch::Architecture::CudaSm90), 3);
    EXPECT_EQ(arch::localIndexOf(arch::Architecture::MpsM2), 1);
    EXPECT_EQ(arch::localIndexOf(arch::Architecture::MpsM3), 2);
    EXPECT_EQ(arch::localIndexOf(arch::Architecture::MpsM4), 3);
}

TEST(ArchitectureMetadata, BackendAssociationMatches) {
    EXPECT_EQ(arch::backendOf(arch::Architecture::CudaSm80), backend::Backend::Cuda);
    EXPECT_EQ(arch::backendOf(arch::Architecture::MpsM3), backend::Backend::Mps);
    EXPECT_EQ(arch::backendOf(arch::Architecture::CpuZen4), backend::Backend::Cpu);
}

TEST(ArchitectureMetadata, IdAndDisplayNameMatchYaml) {
    EXPECT_EQ(arch::idOf(arch::Architecture::CudaSm80), std::string_view("Sm80"));
    EXPECT_EQ(arch::displayNameOf(arch::Architecture::CudaSm80), std::string_view("CUDA SM80"));
    EXPECT_EQ(arch::descriptionOf(arch::Architecture::CudaSm80),
              std::string_view("Ampere 世代 GPU (A100 など) 向け最適化"));

    EXPECT_EQ(arch::idOf(arch::Architecture::CpuSkylake), std::string_view("Skylake"));
    EXPECT_EQ(arch::displayNameOf(arch::Architecture::CpuSkylake), std::string_view("Skylake AVX512"));
}

TEST(ArchitectureLookup, BackendCountsIncludeGeneric) {
    auto verify_backend = [](backend::Backend backend_id) {
        const auto count = arch::countForBackend(backend_id);
        const auto span = arch::architecturesOf(backend_id);
        EXPECT_EQ(count, span.size());
        ASSERT_GE(count, 1u);
        EXPECT_TRUE(arch::isGeneric(span.front()));
    };
    verify_backend(backend::Backend::Cuda);
    verify_backend(backend::Backend::Mps);
    verify_backend(backend::Backend::Cpu);
}

TEST(ArchitectureLookup, ArchitecturesOfReturnsContiguousSpan) {
    const auto cuda_archs = arch::architecturesOf(backend::Backend::Cuda);
    ASSERT_GE(cuda_archs.size(), 4u);
    EXPECT_EQ(cuda_archs.front(), arch::Architecture::CudaGeneric);
    EXPECT_NE(std::find(cuda_archs.begin(), cuda_archs.end(), arch::Architecture::CudaSm80), cuda_archs.end());
    EXPECT_EQ(cuda_archs.back(), arch::Architecture::CudaSm90);

    const auto cpu_archs = arch::architecturesOf(backend::Backend::Cpu);
    ASSERT_GE(cpu_archs.size(), 3u);
    EXPECT_NE(std::find(cpu_archs.begin(), cpu_archs.end(), arch::Architecture::CpuZen4), cpu_archs.end());
    EXPECT_NE(std::find(cpu_archs.begin(), cpu_archs.end(), arch::Architecture::CpuIntelCometLake), cpu_archs.end());
}

TEST(ArchitectureLookup, FromBackendAndLocalIndexRoundsTrip) {
    const auto arch_id = arch::fromBackendAndLocalIndex(backend::Backend::Cuda, 3);
    EXPECT_EQ(arch_id, arch::Architecture::CudaSm90);
    EXPECT_TRUE(arch::hasLocalIndex(backend::Backend::Cuda, 3));
    EXPECT_FALSE(arch::hasLocalIndex(backend::Backend::Cuda, 5));
}

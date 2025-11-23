#include "orteaf/internal/device/device.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

namespace device = orteaf::internal::device;
namespace backend = orteaf::internal::backend;
namespace arch = orteaf::internal::architecture;
namespace dtype = orteaf::internal;
namespace ops = orteaf::internal::ops;

TEST(DeviceBasic, EnumValuesAreDefined) {
    EXPECT_EQ(device::toIndex(device::Device::CudaGeneric), 0u);
    EXPECT_LT(device::toIndex(device::Device::CudaH100Pcie80GB), device::kDeviceCount);
    EXPECT_LT(device::toIndex(device::Device::MpsGeneric), device::kDeviceCount);
    EXPECT_LT(device::toIndex(device::Device::CpuGeneric), device::kDeviceCount);
}

TEST(DeviceBasic, GenericDevicesHaveLocalIndexZero) {
    EXPECT_TRUE(device::isGeneric(device::Device::CudaGeneric));
    EXPECT_TRUE(device::isGeneric(device::Device::MpsGeneric));
    EXPECT_TRUE(device::isGeneric(device::Device::CpuGeneric));
    EXPECT_FALSE(device::isGeneric(device::Device::CudaH100Pcie80GB));
}

TEST(DeviceMetadata, BackendAndArchitectureMatchYaml) {
    EXPECT_EQ(device::backendOf(device::Device::CudaH100Pcie80GB), backend::Backend::Cuda);
    EXPECT_EQ(device::architectureOf(device::Device::CudaH100Pcie80GB),
              arch::Architecture::CudaSm90);

    EXPECT_EQ(device::backendOf(device::Device::MpsM3Max40c), backend::Backend::Mps);
    EXPECT_EQ(device::architectureOf(device::Device::MpsM3Max40c),
              arch::Architecture::MpsM3);
}

TEST(DeviceMetadata, MemoryInfoMatchesConfig) {
    const auto generic = device::memoryOf(device::Device::CudaGeneric);
    EXPECT_EQ(generic.max_bytes, 4294967296ULL);
    EXPECT_EQ(generic.shared_bytes, 49152ULL);

    const auto h100 = device::memoryOf(device::Device::CudaH100Pcie80GB);
    EXPECT_EQ(h100.max_bytes, 85899345920ULL);
    EXPECT_EQ(h100.shared_bytes, 229376ULL);
}

TEST(DeviceMetadata, SupportedDTypesAreOrdered) {
    const auto types = device::supportedDTypes(device::Device::CudaH100Pcie80GB);
    ASSERT_EQ(types.size(), 4u);
    EXPECT_EQ(types[0], dtype::DType::F32);
    EXPECT_EQ(types[1], dtype::DType::F16);
    EXPECT_EQ(types[2], dtype::DType::F8E4M3);
    EXPECT_EQ(types[3], dtype::DType::F8E5M2);
}

TEST(DeviceMetadata, SupportedOpsCoverYamlList) {
    const auto ops_span = device::supportedOps(device::Device::CpuZen4);
    std::vector<ops::Op> ops_vec(ops_span.begin(), ops_span.end());
    ASSERT_EQ(ops_vec.size(), 5u);
    EXPECT_NE(std::find(ops_vec.begin(), ops_vec.end(), ops::Op::Add), ops_vec.end());
    EXPECT_NE(std::find(ops_vec.begin(), ops_vec.end(), ops::Op::MatMul), ops_vec.end());
    EXPECT_NE(std::find(ops_vec.begin(), ops_vec.end(), ops::Op::SpikeThreshold), ops_vec.end());
}

TEST(DeviceMetadata, CapabilitiesExposeKeyValuePairs) {
    const auto caps = device::capabilitiesOf(device::Device::CudaH100Pcie80GB);
    ASSERT_EQ(caps.size(), 3u);
    EXPECT_EQ(caps[0].key, "compute_capability");
    EXPECT_EQ(caps[0].value, "sm90");
    EXPECT_EQ(caps[1].key, "tensor_cores");
    EXPECT_EQ(caps[1].value, "true");
}

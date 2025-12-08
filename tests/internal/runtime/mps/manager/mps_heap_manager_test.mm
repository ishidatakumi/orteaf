#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include <orteaf/internal/diagnostics/error/error.h>
#include <orteaf/internal/runtime/mps/manager/mps_heap_manager.h>
#include <tests/internal/runtime/mps/manager/testing/backend_ops_provider.h>
#include <tests/internal/runtime/mps/manager/testing/manager_test_fixture.h>
#include <tests/internal/testing/error_assert.h>

namespace backend = orteaf::internal::backend;
namespace base = orteaf::internal::base;
namespace diag_error = orteaf::internal::diagnostics::error;
namespace mps_rt = orteaf::internal::runtime::mps::manager;
namespace mps_wrapper = orteaf::internal::runtime::mps::platform::wrapper;
namespace testing_mps = orteaf::tests::runtime::mps::testing;

using orteaf::tests::ExpectError;

namespace {

mps_wrapper::MPSHeap_t makeHeap(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MPSHeap_t>(value);
}

mps_wrapper::MPSHeapDescriptor_t makeHeapDescriptor(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MPSHeapDescriptor_t>(value);
}

template <class Provider>
class MpsHeapManagerTypedTest
    : public testing_mps::RuntimeManagerFixture<Provider,
                                                mps_rt::MpsHeapManager> {
protected:
  using Base =
      testing_mps::RuntimeManagerFixture<Provider, mps_rt::MpsHeapManager>;

  mps_rt::MpsHeapManager &manager() { return Base::manager(); }
  auto &adapter() { return Base::adapter(); }

  mps_rt::HeapDescriptorKey defaultKey(std::size_t size = 0x1000) const {
    mps_rt::HeapDescriptorKey key{};
    key.size_bytes = size;
    return key;
  }

  void
  expectDescriptorConfiguration(const mps_rt::HeapDescriptorKey &key,
                                mps_wrapper::MPSHeapDescriptor_t descriptor,
                                bool expect_creation = true) {
    if constexpr (!Provider::is_mock) {
      (void)key;
      (void)descriptor;
      (void)expect_creation;
      return;
    }
    if (expect_creation) {
      this->adapter().expectCreateHeapDescriptors({descriptor});
    }
    this->adapter().expectSetHeapDescriptorSize({{descriptor, key.size_bytes}});
    this->adapter().expectSetHeapDescriptorResourceOptions(
        {{descriptor, key.resource_options}});
    this->adapter().expectSetHeapDescriptorStorageMode(
        {{descriptor, key.storage_mode}});
    this->adapter().expectSetHeapDescriptorCPUCacheMode(
        {{descriptor, key.cpu_cache_mode}});
    this->adapter().expectSetHeapDescriptorHazardTrackingMode(
        {{descriptor, key.hazard_tracking_mode}});
    this->adapter().expectSetHeapDescriptorType({{descriptor, key.heap_type}});
    this->adapter().expectDestroyHeapDescriptors({descriptor});
  }
};

#if ORTEAF_ENABLE_MPS
using ProviderTypes = ::testing::Types<testing_mps::MockBackendOpsProvider,
                                       testing_mps::RealBackendOpsProvider>;
#else
using ProviderTypes = ::testing::Types<testing_mps::MockBackendOpsProvider>;
#endif

} // namespace

TYPED_TEST_SUITE(MpsHeapManagerTypedTest, ProviderTypes);

TYPED_TEST(MpsHeapManagerTypedTest, GrowthChunkSizeCanBeAdjusted) {
  auto &manager = this->manager();
  EXPECT_EQ(manager.growthChunkSize(), 1u);
  manager.setGrowthChunkSize(3);
  EXPECT_EQ(manager.growthChunkSize(), 3u);
}

TYPED_TEST(MpsHeapManagerTypedTest, GrowthChunkSizeRejectsZero) {
  auto &manager = this->manager();
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { manager.setGrowthChunkSize(0); });
}

TYPED_TEST(MpsHeapManagerTypedTest, GrowthChunkSizeReflectedInDebugState) {
  auto &manager = this->manager();
  manager.setGrowthChunkSize(2);
  const auto device = this->adapter().device();
  manager.initialize(device, this->getOps(), 0);
  const auto key = this->defaultKey();
  if constexpr (TypeParam::is_mock) {
    const auto descriptor = makeHeapDescriptor(0x1501);
    this->expectDescriptorConfiguration(key, descriptor);
    this->adapter().expectCreateHeapsInOrder({{descriptor, makeHeap(0x501)}});
  }
  auto lease = manager.acquire(key);
  const auto snapshot = manager.debugState(lease.handle());
  EXPECT_EQ(snapshot.growth_chunk_size, 2u);
  lease.release();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyHeaps({makeHeap(0x501)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsHeapManagerTypedTest, AccessBeforeInitializationThrows) {
  auto &manager = this->manager();
  const auto key = this->defaultKey();
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.acquire(key); });
}

TYPED_TEST(MpsHeapManagerTypedTest, InitializeRejectsNullDevice) {
  auto &manager = this->manager();
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { manager.initialize(nullptr, this->getOps(), 1); });
}

TYPED_TEST(MpsHeapManagerTypedTest, InitializeRejectsCapacityAboveLimit) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
    manager.initialize(device, this->getOps(),
                       std::numeric_limits<std::size_t>::max());
  });
}

TYPED_TEST(MpsHeapManagerTypedTest, CapacityReflectsConfiguredPool) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  EXPECT_EQ(manager.capacity(), 0u);
  manager.initialize(device, this->getOps(), 2);
  EXPECT_EQ(manager.capacity(), 2u);
  manager.shutdown();
  EXPECT_EQ(manager.capacity(), 0u);
}

TYPED_TEST(MpsHeapManagerTypedTest, GrowthChunkControlsPoolExpansion) {
  auto &manager = this->manager();
  manager.setGrowthChunkSize(3);
  const auto device = this->adapter().device();
  manager.initialize(device, this->getOps(), 0);
  const auto key = this->defaultKey();
  if constexpr (TypeParam::is_mock) {
    const auto descriptor = makeHeapDescriptor(0x1600);
    this->expectDescriptorConfiguration(key, descriptor);
    this->adapter().expectCreateHeapsInOrder({{descriptor, makeHeap(0x600)}});
  }
  auto lease = manager.acquire(key);
  EXPECT_EQ(manager.capacity(), 3u);
  lease.release();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyHeaps({makeHeap(0x600)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsHeapManagerTypedTest, GetOrCreateCachesByDescriptor) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  manager.initialize(device, this->getOps(), 1);
  const auto key = this->defaultKey();
  if constexpr (TypeParam::is_mock) {
    const auto descriptor = makeHeapDescriptor(0x1700);
    this->expectDescriptorConfiguration(key, descriptor);
    this->adapter().expectCreateHeapsInOrder({{descriptor, makeHeap(0x700)}});
  }
  auto first = manager.acquire(key);
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.acquire(key); });
  const auto snapshot = manager.debugState(first.handle());
  EXPECT_TRUE(snapshot.alive);
  EXPECT_EQ(snapshot.size_bytes, key.size_bytes);
  first.release();
  auto second = manager.acquire(key);
  EXPECT_NE(first.handle(), second.handle());
  second.release();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyHeaps({makeHeap(0x700)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsHeapManagerTypedTest, DistinctDescriptorsAllocateSeparateHeaps) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  manager.initialize(device, this->getOps(), 0);
  auto key_a = this->defaultKey(0x1800);
  auto key_b = this->defaultKey(0x2800);
  key_b.storage_mode = mps_wrapper::kMPSStorageModePrivate;
  key_b.heap_type = mps_wrapper::kMPSHeapTypePlacement;
  if constexpr (TypeParam::is_mock) {
    const auto descriptor_a = makeHeapDescriptor(0x1801);
    const auto descriptor_b = makeHeapDescriptor(0x2802);
    this->adapter().expectCreateHeapDescriptors({descriptor_a, descriptor_b});
    this->expectDescriptorConfiguration(key_a, descriptor_a, false);
    this->expectDescriptorConfiguration(key_b, descriptor_b, false);
    this->adapter().expectCreateHeapsInOrder(
        {{descriptor_a, makeHeap(0x801)}, {descriptor_b, makeHeap(0x802)}});
  }
  auto lease_a = manager.acquire(key_a);
  auto lease_b = manager.acquire(key_b);
  EXPECT_NE(lease_a.handle(), lease_b.handle());
  const auto snapshot_b = manager.debugState(lease_b.handle());
  EXPECT_EQ(snapshot_b.storage_mode, key_b.storage_mode);
  EXPECT_EQ(snapshot_b.heap_type, key_b.heap_type);
  lease_a.release();
  lease_b.release();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyHeaps({makeHeap(0x801), makeHeap(0x802)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsHeapManagerTypedTest, ReleaseAllowsReuseWithoutRecreation) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  manager.initialize(device, this->getOps(), 1);
  const auto key = this->defaultKey();
  if constexpr (TypeParam::is_mock) {
    const auto descriptor_first = makeHeapDescriptor(0x1900);
    this->expectDescriptorConfiguration(key, descriptor_first);
    this->adapter().expectCreateHeapsInOrder(
        {{descriptor_first, makeHeap(0x900)}});
  }
  auto lease = manager.acquire(key);
  lease.release();
  auto recreated = manager.acquire(key);
  EXPECT_NE(lease.handle(), recreated.handle());
  recreated.release();
  if constexpr (TypeParam::is_mock) {
    this->adapter().expectDestroyHeaps({makeHeap(0x900)});
  }
  manager.shutdown();
}

TYPED_TEST(MpsHeapManagerTypedTest, ManualReleaseInvalidatesLease) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  manager.initialize(device, this->getOps(), 1);
  const auto key = this->defaultKey();
  if constexpr (TypeParam::is_mock) {
    const auto descriptor = makeHeapDescriptor(0x1D00);
    this->expectDescriptorConfiguration(key, descriptor);
    this->adapter().expectCreateHeapsInOrder({{descriptor, makeHeap(0xD00)}});
    this->adapter().expectDestroyHeaps({makeHeap(0xD00)});
  }

  auto lease = manager.acquire(key);
  const auto handle = lease.handle();

  manager.release(lease);
  EXPECT_FALSE(static_cast<bool>(lease));
  EXPECT_GT(manager.debugState(handle).generation, 0u);

  manager.shutdown();
}

TYPED_TEST(MpsHeapManagerTypedTest, DescriptorSizeMustBePositive) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  manager.initialize(device, this->getOps(), 0);
  mps_rt::HeapDescriptorKey key{};
  key.size_bytes = 0;
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { (void)manager.acquire(key); });
  manager.shutdown();
}

TYPED_TEST(MpsHeapManagerTypedTest, ShutdownDestroysRemainingHeaps) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  manager.initialize(device, this->getOps(), 1);
  const auto key = this->defaultKey(0x1F00);
  if constexpr (TypeParam::is_mock) {
    const auto descriptor = makeHeapDescriptor(0x1B00);
    this->expectDescriptorConfiguration(key, descriptor);
    this->adapter().expectCreateHeapsInOrder({{descriptor, makeHeap(0xB00)}});
    this->adapter().expectDestroyHeaps({makeHeap(0xB00)});
  }
  (void)manager.acquire(key);
  manager.shutdown();
}

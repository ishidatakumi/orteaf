#include "orteaf/internal/runtime/allocator/resource/mps/mps_resource.h"

#if ORTEAF_ENABLE_MPS

#include <gtest/gtest.h>

#include "orteaf/internal/runtime/mps/manager/mps_library_manager.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_device.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_heap.h"
#include "tests/internal/testing/error_assert.h"

namespace orteaf::tests {
using orteaf::internal::runtime::allocator::resource::mps::MpsResource;
namespace diag_error = ::orteaf::internal::diagnostics::error;
namespace mps = ::orteaf::internal::runtime::mps::platform::wrapper;

TEST(MpsResourceTest, InitializeNullDeviceThrows) {
  MpsResource::Config cfg{};
  cfg.device = nullptr;
  cfg.heap = reinterpret_cast<mps::MpsHeap_t>(0x1);
  ExpectErrorMessage(diag_error::OrteafErrc::NullPointer, {"device"}, [&] {
    MpsResource resource(cfg);
    (void)resource;
  });
}

TEST(MpsResourceTest, InitializeNullHeapThrows) {
  MpsResource::Config cfg{};
  cfg.device = reinterpret_cast<mps::MpsDevice_t>(0x1);
  cfg.heap = nullptr;
  ExpectErrorMessage(diag_error::OrteafErrc::NullPointer, {"heap"}, [&] {
    MpsResource resource(cfg);
    (void)resource;
  });
}

TEST(MpsResourceTest, AllocateBeforeInitializeThrows) {
  MpsResource resource;
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { resource.allocate(16, 0); });
}

TEST(MpsResourceTest, DeallocateEmptyIsNoOp) {
  MpsResource resource;
  EXPECT_NO_THROW(resource.deallocate({}, 0, 0));
}

TEST(MpsResourceTest, AllocateZeroThrowsWhenInitialized) {
  auto device = mps::getDevice();
  if (device == nullptr) {
    GTEST_SKIP() << "MPS device unavailable; skipping zero-size allocate test";
  }

  auto descriptor = mps::createHeapDescriptor();
  if (!descriptor) {
    GTEST_SKIP() << "Failed to create heap descriptor";
  }
  mps::setHeapDescriptorSize(descriptor, 4096);
  auto heap = mps::createHeap(device, descriptor);
  if (!heap) {
    mps::destroyHeapDescriptor(descriptor);
    GTEST_SKIP() << "Failed to create heap";
  }

  MpsResource resource;
  ::orteaf::internal::runtime::mps::manager::MpsLibraryManager dummy_lib_mgr;
  MpsResource::Config cfg{};
  cfg.device_handle = ::orteaf::internal::base::DeviceHandle{0};
  cfg.device = device;
  cfg.heap = heap;
  cfg.usage = mps::kMPSDefaultBufferUsage;
  cfg.library_manager = &dummy_lib_mgr;
  EXPECT_NO_THROW(resource.initialize(cfg));
  ExpectErrorMessage(diag_error::OrteafErrc::InvalidParameter,
                     {"size", "MpsResource"}, [&] { resource.allocate(0, 0); });

  mps::destroyHeap(heap);
  mps::destroyHeapDescriptor(descriptor);
}

} // namespace orteaf::tests

#endif // ORTEAF_ENABLE_MPS

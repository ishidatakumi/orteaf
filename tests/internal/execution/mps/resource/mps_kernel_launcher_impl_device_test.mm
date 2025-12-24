#include <gtest/gtest.h>

#include "orteaf/internal/execution/mps/api/mps_runtime_api.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_buffer.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_command_buffer.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_command_queue.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_device.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_heap.h"
#include "orteaf/internal/execution/mps/resource/mps_kernel_launcher_impl.h"

namespace base = orteaf::internal::base;
namespace mps_api = orteaf::internal::execution::mps::api;
namespace mps_rt = orteaf::internal::execution::mps;

TEST(MpsKernelLauncherImplDeviceTest, InitializeWithEmbeddedLibraryRealDevice) {
  // Skip if no MPS device is available on the machine.
  if (::orteaf::internal::execution::mps::platform::wrapper::getDeviceCount() ==
      0) {
    GTEST_SKIP() << "No MPS devices available";
  }

  mps_api::MpsRuntimeApi::initialize();

  {
    mps_rt::resource::MpsKernelLauncherImpl<1> impl({
        {"embed_test_library", "orteaf_embed_test_identity"},
    });

    const base::DeviceHandle device{0};
    impl.initialize<mps_api::MpsRuntimeApi>(device);

    EXPECT_TRUE(impl.initialized(device));
    ASSERT_EQ(impl.sizeForTest(), 1u);

    auto &lease = impl.pipelineLeaseForTest(device, 0);
    auto *payload = lease.payloadPtr();
    EXPECT_NE(payload, nullptr);
    if (payload) {
      EXPECT_NE(payload->pipeline_state, nullptr);
    }
  } // impl destroyed here, releases all leases

  mps_api::MpsRuntimeApi::shutdown();
}

TEST(MpsKernelLauncherImplDeviceTest, DispatchOneShotExecutesEmbeddedIdentity) {
  if (::orteaf::internal::execution::mps::platform::wrapper::getDeviceCount() ==
      0) {
    GTEST_SKIP() << "No MPS devices available";
  }

  mps_api::MpsRuntimeApi::initialize();

  auto *device_handle =
      ::orteaf::internal::execution::mps::platform::wrapper::getDevice(0);
  ASSERT_NE(device_handle, nullptr);
  ::orteaf::internal::execution::mps::platform::MpsSlowOpsImpl slow_ops{};
  ::orteaf::internal::execution::mps::manager::MpsCommandQueueManager
      queue_manager{};
  queue_manager.configure(
      ::orteaf::internal::execution::mps::manager::MpsCommandQueueManager::Config{device_handle, &slow_ops, 1, 1, 1, 1, 1, 1});
  auto queue_lease = queue_manager.acquire();

  // Create a shared heap and buffer.
  auto *desc = ::orteaf::internal::execution::mps::platform::wrapper::
      createHeapDescriptor();
  ASSERT_NE(desc, nullptr);
  ::orteaf::internal::execution::mps::platform::wrapper::setHeapDescriptorSize(
      desc, 4096);
  ::orteaf::internal::execution::mps::platform::wrapper::
      setHeapDescriptorStorageMode(desc,
                                   ::orteaf::internal::execution::mps::platform::
                                       wrapper::kMPSStorageModeShared);
  auto *heap = ::orteaf::internal::execution::mps::platform::wrapper::createHeap(
      device_handle, desc);
  ASSERT_NE(heap, nullptr);

  constexpr std::size_t kCount = 16;
  auto *buffer =
      ::orteaf::internal::execution::mps::platform::wrapper::createBuffer(
          heap, kCount * sizeof(float));
  ASSERT_NE(buffer, nullptr);

  // Initialize buffer contents.
  float *data = static_cast<float *>(
      ::orteaf::internal::execution::mps::platform::wrapper::getBufferContents(
          buffer));
  for (std::size_t i = 0; i < kCount; ++i) {
    data[i] = static_cast<float>(i);
  }
  const uint32_t length = static_cast<uint32_t>(kCount);

  ::orteaf::internal::execution::mps::platform::wrapper::MPSSize_t tg{kCount, 1,
                                                                    1};
  ::orteaf::internal::execution::mps::platform::wrapper::MPSSize_t tptg{1, 1, 1};

  {
    mps_rt::resource::MpsKernelLauncherImpl<1> impl({
        {"embed_test_library", "orteaf_embed_test_identity"},
    });

    const base::DeviceHandle device{0};
    impl.initialize<mps_api::MpsRuntimeApi>(device);

    auto *command_buffer = impl.dispatchOneShot<
        ::orteaf::internal::execution::mps::platform::MpsFastOps,
        mps_api::MpsRuntimeApi>(
        queue_lease, device, 0, tg, tptg, [&](auto *encoder) {
          impl.setBuffer<>(encoder, buffer, 0, 0);
          impl.setBytes<>(encoder, &length, sizeof(length), 1);
        });

    ASSERT_NE(command_buffer, nullptr);
    ::orteaf::internal::execution::mps::platform::wrapper::waitUntilCompleted(
        command_buffer);

    // Identity kernel should leave data unchanged.
    for (std::size_t i = 0; i < kCount; ++i) {
      EXPECT_FLOAT_EQ(data[i], static_cast<float>(i));
    }

    ::orteaf::internal::execution::mps::platform::wrapper::destroyCommandBuffer(
        command_buffer);
  } // impl destroyed here, releases all leases

  queue_lease.release();
  queue_manager.shutdown();

  ::orteaf::internal::execution::mps::platform::wrapper::destroyBuffer(buffer);
  ::orteaf::internal::execution::mps::platform::wrapper::destroyHeap(heap);
  ::orteaf::internal::execution::mps::platform::wrapper::destroyHeapDescriptor(
      desc);
  ::orteaf::internal::execution::mps::platform::wrapper::deviceRelease(
      device_handle);

  mps_api::MpsRuntimeApi::shutdown();
}

#include <gtest/gtest.h>

#include "orteaf/internal/runtime/kernel/mps/mps_kernel_launcher_impl.h"
#include "orteaf/internal/runtime/mps/ops/mps_public_ops.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_buffer.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_command_buffer.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_command_queue.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_device.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_heap.h"
#include "orteaf/internal/runtime/mps/ops/mps_private_ops.h"

namespace base = orteaf::internal::base;
namespace mps_rt = orteaf::internal::runtime::mps;

TEST(MpsKernelLauncherImplDeviceTest, InitializeWithEmbeddedLibraryRealDevice) {
  // Skip if no MPS device is available on the machine.
  if (::orteaf::internal::runtime::mps::platform::wrapper::getDeviceCount() ==
      0) {
    GTEST_SKIP() << "No MPS devices available";
  }

  ::orteaf::internal::runtime::mps::ops::MpsPublicOps public_ops;
  public_ops.initialize();

  mps_rt::MpsKernelLauncherImpl<1> impl({
      {"embed_test_library", "orteaf_embed_test_identity"},
  });

  const base::DeviceHandle device{0};
  impl.initialize<>(device);

  EXPECT_TRUE(impl.initialized(device));
  ASSERT_EQ(impl.sizeForTest(), 1u);

  auto &lease = impl.pipelineLeaseForTest(device, 0);
  EXPECT_NE(lease.pointer(), nullptr);

  public_ops.shutdown();
}

TEST(MpsKernelLauncherImplDeviceTest, DispatchOneShotExecutesEmbeddedIdentity) {
  if (::orteaf::internal::runtime::mps::platform::wrapper::getDeviceCount() ==
      0) {
    GTEST_SKIP() << "No MPS devices available";
  }

  ::orteaf::internal::runtime::mps::ops::MpsPublicOps public_ops;
  public_ops.initialize();

  mps_rt::MpsKernelLauncherImpl<1> impl({
      {"embed_test_library", "orteaf_embed_test_identity"},
  });

  const base::DeviceHandle device{0};
  impl.initialize<>(device);

  auto *device_handle =
      ::orteaf::internal::runtime::mps::platform::wrapper::getDevice(0);
  ASSERT_NE(device_handle, nullptr);
  auto *queue =
      ::orteaf::internal::runtime::mps::platform::wrapper::createCommandQueue(
          device_handle);
  ASSERT_NE(queue, nullptr);

  // Create a shared heap and buffer.
  auto *desc = ::orteaf::internal::runtime::mps::platform::wrapper::
      createHeapDescriptor();
  ASSERT_NE(desc, nullptr);
  ::orteaf::internal::runtime::mps::platform::wrapper::setHeapDescriptorSize(
      desc, 4096);
  ::orteaf::internal::runtime::mps::platform::wrapper::
      setHeapDescriptorStorageMode(desc,
                                   ::orteaf::internal::runtime::mps::platform::
                                       wrapper::kMPSStorageModeShared);
  auto *heap = ::orteaf::internal::runtime::mps::platform::wrapper::createHeap(
      device_handle, desc);
  ASSERT_NE(heap, nullptr);

  constexpr std::size_t kCount = 16;
  auto *buffer =
      ::orteaf::internal::runtime::mps::platform::wrapper::createBuffer(
          heap, kCount * sizeof(float));
  ASSERT_NE(buffer, nullptr);

  // Initialize buffer contents.
  float *data = static_cast<float *>(
      ::orteaf::internal::runtime::mps::platform::wrapper::getBufferContents(
          buffer));
  for (std::size_t i = 0; i < kCount; ++i) {
    data[i] = static_cast<float>(i);
  }
  const uint32_t length = static_cast<uint32_t>(kCount);

  ::orteaf::internal::runtime::mps::platform::wrapper::MPSSize_t tg{kCount, 1,
                                                                    1};
  ::orteaf::internal::runtime::mps::platform::wrapper::MPSSize_t tptg{1, 1, 1};

  auto queue_lease =
      ::orteaf::internal::runtime::mps::manager::MpsCommandQueueManager::
          CommandQueueLease::makeForTest(base::CommandQueueHandle{0}, queue);

  auto *command_buffer = impl.dispatchOneShot<>(
      queue_lease, device, 0, tg, tptg, [&](auto *encoder) {
        impl.setBuffer<>(encoder, buffer, 0, 0);
        impl.setBytes<>(encoder, &length, sizeof(length), 1);
      });

  ASSERT_NE(command_buffer, nullptr);
  ::orteaf::internal::runtime::mps::platform::wrapper::waitUntilCompleted(
      command_buffer);

  // Identity kernel should leave data unchanged.
  for (std::size_t i = 0; i < kCount; ++i) {
    EXPECT_FLOAT_EQ(data[i], static_cast<float>(i));
  }

  ::orteaf::internal::runtime::mps::platform::wrapper::destroyCommandBuffer(
      command_buffer);
  ::orteaf::internal::runtime::mps::platform::wrapper::destroyBuffer(buffer);
  ::orteaf::internal::runtime::mps::platform::wrapper::destroyHeap(heap);
  ::orteaf::internal::runtime::mps::platform::wrapper::destroyHeapDescriptor(
      desc);
  ::orteaf::internal::runtime::mps::platform::wrapper::destroyCommandQueue(
      queue);
  ::orteaf::internal::runtime::mps::platform::wrapper::deviceRelease(
      device_handle);

  public_ops.shutdown();
}

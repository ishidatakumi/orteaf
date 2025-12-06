#include <gtest/gtest.h>

#include "orteaf/internal/runtime/kernel/mps/mps_kernel_launcher_impl.h"
#include "orteaf/internal/runtime/ops/mps/public/mps_public_ops.h"
#include "orteaf/internal/runtime/ops/mps/private/mps_private_ops.h"
#include "orteaf/internal/backend/mps/wrapper/mps_device.h"
#include "orteaf/internal/backend/mps/wrapper/mps_command_queue.h"
#include "orteaf/internal/backend/mps/wrapper/mps_command_buffer.h"
#include "orteaf/internal/backend/mps/wrapper/mps_heap.h"
#include "orteaf/internal/backend/mps/wrapper/mps_buffer.h"

namespace base = orteaf::internal::base;
namespace mps_rt = orteaf::internal::runtime::mps;

TEST(MpsKernelLauncherImplDeviceTest, InitializeWithEmbeddedLibraryRealDevice) {
    // Skip if no MPS device is available on the machine.
    if (::orteaf::internal::backend::mps::getDeviceCount() == 0) {
        GTEST_SKIP() << "No MPS devices available";
    }

    ::orteaf::internal::runtime::ops::mps::MpsPublicOps public_ops;
    public_ops.initialize();

    mps_rt::MpsKernelLauncherImpl<1> impl({
        {"embed_test_library", "orteaf_embed_test_identity"},
    });

    const base::DeviceHandle device{0};
    impl.initialize<>(device);

    EXPECT_TRUE(impl.initialized());
    ASSERT_EQ(impl.sizeForTest(), 1u);

    auto& lease = impl.pipelineLeaseForTest(0);
    EXPECT_NE(lease.pointer(), nullptr);

    public_ops.shutdown();
}

TEST(MpsKernelLauncherImplDeviceTest, DispatchOneShotExecutesEmbeddedIdentity) {
    if (::orteaf::internal::backend::mps::getDeviceCount() == 0) {
        GTEST_SKIP() << "No MPS devices available";
    }

    ::orteaf::internal::runtime::ops::mps::MpsPublicOps public_ops;
    public_ops.initialize();

    mps_rt::MpsKernelLauncherImpl<1> impl({
        {"embed_test_library", "orteaf_embed_test_identity"},
    });

    const base::DeviceHandle device{0};
    impl.initialize<>(device);

    auto* device_handle = ::orteaf::internal::backend::mps::getDevice(0);
    ASSERT_NE(device_handle, nullptr);
    auto* queue = ::orteaf::internal::backend::mps::createCommandQueue(device_handle);
    ASSERT_NE(queue, nullptr);

    // Create a shared heap and buffer.
    auto* desc = ::orteaf::internal::backend::mps::createHeapDescriptor();
    ASSERT_NE(desc, nullptr);
    ::orteaf::internal::backend::mps::setHeapDescriptorSize(desc, 4096);
    ::orteaf::internal::backend::mps::setHeapDescriptorStorageMode(
        desc, ::orteaf::internal::backend::mps::kMPSStorageModeShared);
    auto* heap = ::orteaf::internal::backend::mps::createHeap(device_handle, desc);
    ASSERT_NE(heap, nullptr);

    constexpr std::size_t kCount = 16;
    auto* buffer = ::orteaf::internal::backend::mps::createBuffer(heap, kCount * sizeof(float));
    ASSERT_NE(buffer, nullptr);

    // Initialize buffer contents.
    float* data = static_cast<float*>(::orteaf::internal::backend::mps::getBufferContents(buffer));
    for (std::size_t i = 0; i < kCount; ++i) {
        data[i] = static_cast<float>(i);
    }
    const uint32_t length = static_cast<uint32_t>(kCount);

    ::orteaf::internal::backend::mps::MPSSize_t tg{kCount, 1, 1};
    ::orteaf::internal::backend::mps::MPSSize_t tptg{1, 1, 1};

    auto* command_buffer = impl.dispatchOneShot<>(queue, 0, tg, tptg, [&](auto* encoder) {
        impl.setBuffer<>(encoder, buffer, 0, 0);
        impl.setBytes<>(encoder, &length, sizeof(length), 1);
    });

    ASSERT_NE(command_buffer, nullptr);
    ::orteaf::internal::backend::mps::waitUntilCompleted(command_buffer);

    // Identity kernel should leave data unchanged.
    for (std::size_t i = 0; i < kCount; ++i) {
        EXPECT_FLOAT_EQ(data[i], static_cast<float>(i));
    }

    ::orteaf::internal::backend::mps::destroyCommandBuffer(command_buffer);
    ::orteaf::internal::backend::mps::destroyBuffer(buffer);
    ::orteaf::internal::backend::mps::destroyHeap(heap);
    ::orteaf::internal::backend::mps::destroyHeapDescriptor(desc);
    ::orteaf::internal::backend::mps::destroyCommandQueue(queue);
    ::orteaf::internal::backend::mps::deviceRelease(device_handle);

    public_ops.shutdown();
}

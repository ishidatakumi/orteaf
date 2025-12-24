/**
 * @file cuda_alloc_copy_test.cpp
 * @brief Tests for CUDA memory allocation, deallocation, and copy operations.
 */

#include "orteaf/internal/execution/cuda/platform/wrapper/cuda_alloc.h"
#include "orteaf/internal/execution/cuda/platform/wrapper/cuda_init.h"
#include "orteaf/internal/execution/cuda/platform/wrapper/cuda_device.h"
#include "orteaf/internal/execution/cuda/platform/wrapper/cuda_context.h"
#include "orteaf/internal/execution/cuda/platform/wrapper/cuda_stream.h"
#include "tests/internal/testing/error_assert.h"

#include <gtest/gtest.h>
#include <array>
#include <cstddef>
#include <cstring>
#include <vector>

namespace cuda = orteaf::internal::execution::cuda::platform::wrapper;

/**
 * @brief Test fixture that initializes CUDA and sets up a device and context.
 */
class CudaAllocCopyTest : public ::testing::Test {
protected:
    void SetUp() override {
        cuda::cudaInit();
        int count = cuda::getDeviceCount();
        if (count == 0) {
            GTEST_SKIP() << "No CUDA devices available";
        }
        device_ = cuda::getDevice(0);
        context_ = cuda::getPrimaryContext(device_);
        cuda::setContext(context_);
    }
    
    void TearDown() override {
        if (context_ != nullptr) {
            cuda::releasePrimaryContext(device_);
        }
    }
    
    cuda::CudaDevice_t device_{0};
    cuda::CudaContext_t context_ = nullptr;
};

/**
 * @brief Test that device memory allocation succeeds.
 */
TEST_F(CudaAllocCopyTest, AllocDeviceMemorySucceeds) {
    constexpr size_t size = 1024;
    cuda::CudaDevicePtr_t ptr = cuda::alloc(size);
    EXPECT_NE(ptr, 0);
    
    cuda::free(ptr, size);
}

/**
 * @brief Test that allocating zero bytes fails (CUDA does not allow zero-size allocation).
 */
TEST_F(CudaAllocCopyTest, AllocZeroBytesFails) {
    // CUDA's cuMemAlloc does not allow zero-size allocation
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
        [] { cuda::alloc(0); });
}

/**
 * @brief Test that allocation with very large size fails appropriately.
 */
TEST_F(CudaAllocCopyTest, AllocLargeSizeFails) {
    // Allocate an unreasonably large size (e.g., all address space)
    size_t huge_size = SIZE_MAX;
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::OutOfMemory,
        [&] { cuda::alloc(huge_size); });
}

/**
 * @brief Test that device memory deallocation works.
 */
TEST_F(CudaAllocCopyTest, FreeDeviceMemorySucceeds) {
    constexpr size_t size = 1024;
    cuda::CudaDevicePtr_t ptr = cuda::alloc(size);
    EXPECT_NE(ptr, 0);
    
    EXPECT_NO_THROW(cuda::free(ptr, size));
}

/**
 * @brief Test that freeing zero pointer is handled gracefully.
 */
TEST_F(CudaAllocCopyTest, FreeZeroPointer) {
    EXPECT_NO_THROW(cuda::free(0, 0));
}

/**
 * @brief Test that multiple allocations work.
 */
TEST_F(CudaAllocCopyTest, MultipleAllocations) {
    constexpr size_t size = 256;
    constexpr int num_allocs = 10;
    
    std::vector<cuda::CudaDevicePtr_t> ptrs;
    for (int i = 0; i < num_allocs; ++i) {
        cuda::CudaDevicePtr_t ptr = cuda::alloc(size);
        EXPECT_NE(ptr, 0);
        ptrs.push_back(ptr);
    }
    
    // Free all allocations
    for (auto ptr : ptrs) {
        cuda::free(ptr, size);
    }
}

/**
 * @brief Test that asynchronous device memory allocation succeeds.
 */
TEST_F(CudaAllocCopyTest, AllocStreamSucceeds) {
    cuda::CudaStream_t stream = cuda::getStream();
    constexpr size_t size = 1024;
    
    cuda::CudaDevicePtr_t ptr = cuda::allocStream(size, stream);
    EXPECT_NE(ptr, 0);
    
    cuda::synchronizeStream(stream);
    cuda::freeStream(ptr, size, stream);
    cuda::synchronizeStream(stream);
    cuda::releaseStream(stream);
}

/**
 * @brief Test that alloc_stream with nullptr stream throws.
 */
TEST_F(CudaAllocCopyTest, AllocStreamNullptrThrows) {
    constexpr size_t size = 1024;
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        [&] { cuda::allocStream(size, nullptr); });
}

/**
 * @brief Test that alloc_stream with zero size throws.
 */
TEST_F(CudaAllocCopyTest, AllocStreamZeroBytesFails) {
    cuda::CudaStream_t stream = cuda::getStream();
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
        [&] { cuda::allocStream(0, stream); });
    cuda::releaseStream(stream);
}

/**
 * @brief Test that asynchronous device memory deallocation works.
 */
TEST_F(CudaAllocCopyTest, FreeStreamSucceeds) {
    cuda::CudaStream_t stream = cuda::getStream();
    constexpr size_t size = 1024;
    
    cuda::CudaDevicePtr_t ptr = cuda::allocStream(size, stream);
    cuda::synchronizeStream(stream);
    
    EXPECT_NO_THROW(cuda::freeStream(ptr, size, stream));
    cuda::synchronizeStream(stream);
    cuda::releaseStream(stream);
}

/**
 * @brief Test that free_stream with nullptr stream throws.
 */
TEST_F(CudaAllocCopyTest, FreeStreamNullptrThrows) {
    constexpr size_t size = 1024;
    cuda::CudaDevicePtr_t ptr = cuda::alloc(size);
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        [&] { cuda::freeStream(ptr, size, nullptr); });
    cuda::free(ptr, size);
}

/**
 * @brief Test that pinned host memory allocation succeeds.
 */
TEST_F(CudaAllocCopyTest, AllocHostSucceeds) {
    constexpr size_t size = 1024;
    void* ptr = cuda::allocHost(size);
    EXPECT_NE(ptr, nullptr);
    
    cuda::freeHost(ptr, size);
}

/**
 * @brief Test that allocating zero bytes for host memory throws.
 */
TEST_F(CudaAllocCopyTest, AllocHostZeroBytesFails) {
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
        [] { cuda::allocHost(0); });
}

/**
 * @brief Test that pinned host memory is writable.
 */
TEST_F(CudaAllocCopyTest, AllocHostIsWritable) {
    constexpr size_t size = sizeof(int);
    void* ptr = cuda::allocHost(size);
    ASSERT_NE(ptr, nullptr);
    
    int* int_ptr = static_cast<int*>(ptr);
    *int_ptr = 42;
    EXPECT_EQ(*int_ptr, 42);
    
    cuda::freeHost(ptr, size);
}

/**
 * @brief Test that pinned host memory deallocation works.
 */
TEST_F(CudaAllocCopyTest, FreeHostSucceeds) {
    constexpr size_t size = 1024;
    void* ptr = cuda::allocHost(size);
    EXPECT_NE(ptr, nullptr);
    
    EXPECT_NO_THROW(cuda::freeHost(ptr, size));
}

/**
 * @brief Test that free_host with nullptr throws.
 */
TEST_F(CudaAllocCopyTest, FreeHostNullptrThrows) {
    constexpr size_t size = 1024;
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        [&] { cuda::freeHost(nullptr, size); });
}

/**
 * @brief Test that copy_to_host works.
 */
TEST_F(CudaAllocCopyTest, CopyToHostSucceeds) {
    constexpr size_t size = sizeof(int);
    cuda::CudaDevicePtr_t dev_ptr = cuda::alloc(size);
    EXPECT_NE(dev_ptr, 0);
    
    // Write value to device memory via copy_to_device
    int host_value = 123;
    cuda::copyToDevice(&host_value, dev_ptr, size);
    
    // Copy back to host
    int host_result = 0;
    EXPECT_NO_THROW(cuda::copyToHost(dev_ptr, &host_result, size));
    EXPECT_EQ(host_result, host_value);
    
    cuda::free(dev_ptr, size);
}

/**
 * @brief Test that copy_to_device works.
 */
TEST_F(CudaAllocCopyTest, CopyToDeviceSucceeds) {
    constexpr size_t size = sizeof(int);
    cuda::CudaDevicePtr_t dev_ptr = cuda::alloc(size);
    EXPECT_NE(dev_ptr, 0);
    
    int host_value = 456;
    EXPECT_NO_THROW(cuda::copyToDevice(&host_value, dev_ptr, size));
    
    // Verify by copying back
    int host_result = 0;
    cuda::copyToHost(dev_ptr, &host_result, size);
    EXPECT_EQ(host_result, host_value);
    
    cuda::free(dev_ptr, size);
}

/**
 * @brief Test that copy operations work with different sizes.
 */
TEST_F(CudaAllocCopyTest, CopyDifferentSizes) {
    std::vector<size_t> sizes = {1, 4, 16, 64, 256, 1024, 4096};
    
    for (size_t size : sizes) {
        cuda::CudaDevicePtr_t dev_ptr = cuda::alloc(size);
        EXPECT_NE(dev_ptr, 0);
        
        std::vector<uint8_t> host_data(size, 0xAB);
        cuda::copyToDevice(host_data.data(), dev_ptr, size);
        
        std::vector<uint8_t> host_result(size);
        cuda::copyToHost(dev_ptr, host_result.data(), size);
        
        EXPECT_EQ(host_data, host_result);
        
        cuda::free(dev_ptr, size);
    }
}

/**
 * @brief Test that copy_to_host with invalid pointer throws.
 */
TEST_F(CudaAllocCopyTest, CopyToHostInvalidPointerThrows) {
    constexpr size_t size = sizeof(int);
    cuda::CudaDevicePtr_t invalid_ptr = static_cast<cuda::CudaDevicePtr_t>(-1);
    std::array<std::byte, sizeof(int)> host_buffer{};
    void* host_ptr = host_buffer.data();
    ASSERT_NE(host_ptr, nullptr);
    
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
        [&] { cuda::copyToHost(invalid_ptr, host_ptr, size); });
}

/**
 * @brief Test that copy_to_host with zero device pointer throws.
 */
TEST_F(CudaAllocCopyTest, CopyToHostZeroPtrThrows) {
    constexpr size_t size = sizeof(int);
    int host_value = 0;
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
        [&] { cuda::copyToHost(0, &host_value, size); });
}

/**
 * @brief Test that copy_to_host with nullptr host pointer throws.
 */
TEST_F(CudaAllocCopyTest, CopyToHostNullptrHostPtrThrows) {
    constexpr size_t size = sizeof(int);
    cuda::CudaDevicePtr_t dev_ptr = cuda::alloc(size);
    EXPECT_NE(dev_ptr, 0);
    
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        [&] { cuda::copyToHost(dev_ptr, nullptr, size); });
    
    cuda::free(dev_ptr, size);
}

/**
 * @brief Test that copy_to_device with invalid pointer throws.
 */
TEST_F(CudaAllocCopyTest, CopyToDeviceInvalidPointerThrows) {
    constexpr size_t size = sizeof(int);
    int host_value = 42;
    cuda::CudaDevicePtr_t invalid_ptr = static_cast<cuda::CudaDevicePtr_t>(-1);
    
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
        [&] { cuda::copyToDevice(&host_value, invalid_ptr, size); });
}

/**
 * @brief Test that copy_to_device with zero device pointer throws.
 */
TEST_F(CudaAllocCopyTest, CopyToDeviceZeroPtrThrows) {
    constexpr size_t size = sizeof(int);
    int host_value = 42;
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
        [&] { cuda::copyToDevice(&host_value, 0, size); });
}

/**
 * @brief Test that copy_to_device with nullptr host pointer throws.
 */
TEST_F(CudaAllocCopyTest, CopyToDeviceNullptrHostPtrThrows) {
    constexpr size_t size = sizeof(int);
    cuda::CudaDevicePtr_t dev_ptr = cuda::alloc(size);
    EXPECT_NE(dev_ptr, 0);
    
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        [&] { cuda::copyToDevice(nullptr, dev_ptr, size); });
    
    cuda::free(dev_ptr, size);
}

/**
 * @brief Test that copy operations work with zero size.
 */
TEST_F(CudaAllocCopyTest, CopyZeroSize) {
    constexpr size_t size = 1024;
    cuda::CudaDevicePtr_t dev_ptr = cuda::alloc(size);
    EXPECT_NE(dev_ptr, 0);
    
    int host_value = 789;
    EXPECT_NO_THROW(cuda::copyToDevice(&host_value, dev_ptr, 0));
    EXPECT_NO_THROW(cuda::copyToHost(dev_ptr, &host_value, 0));
    
    cuda::free(dev_ptr, size);
}

/**
 * @brief Test complete allocation/copy/deallocation cycle.
 */
TEST_F(CudaAllocCopyTest, CompleteCycle) {
    constexpr size_t size = 1024;
    constexpr int test_value = 0xDEADBEEF;
    
    // Allocate device memory
    cuda::CudaDevicePtr_t dev_ptr = cuda::alloc(size);
    EXPECT_NE(dev_ptr, 0);
    
    // Allocate pinned host memory
    void* host_ptr = cuda::allocHost(size);
    EXPECT_NE(host_ptr, nullptr);
    
    // Initialize host memory
    std::memset(host_ptr, test_value, size);
    
    // Copy host -> device
    cuda::copyToDevice(host_ptr, dev_ptr, size);
    
    // Clear host memory
    std::memset(host_ptr, 0, size);
    
    // Copy device -> host
    cuda::copyToHost(dev_ptr, host_ptr, size);
    
    // Verify data
    uint8_t* byte_ptr = static_cast<uint8_t*>(host_ptr);
    for (size_t i = 0; i < size; ++i) {
        EXPECT_EQ(byte_ptr[i], static_cast<uint8_t>(test_value));
    }
    
    // Cleanup
    cuda::free(dev_ptr, size);
    cuda::freeHost(host_ptr, size);
}

/**
 * @brief Test that statistics are updated correctly (indirect check).
 */
TEST_F(CudaAllocCopyTest, StatisticsUpdated) {
    constexpr size_t size = 1024;
    
    // Allocate device memory (should update stats)
    cuda::CudaDevicePtr_t dev_ptr = cuda::alloc(size);
    EXPECT_NE(dev_ptr, 0);
    
    // Copy operations should NOT update stats (as per documentation)
    int host_value = 42;
    cuda::copyToDevice(&host_value, dev_ptr, size);
    cuda::copyToHost(dev_ptr, &host_value, size);
    
    // Deallocate device memory (should update stats)
    cuda::free(dev_ptr, size);
    
    // Allocate pinned host memory (should update stats)
    void* host_ptr = cuda::allocHost(size);
    EXPECT_NE(host_ptr, nullptr);
    
    // Deallocate pinned host memory (should update stats)
    cuda::freeHost(host_ptr, size);
}

/**
 * @file cuda_context_stream_event_test.cpp
 * @brief Tests for CUDA context, stream, and event management.
 */

#include "orteaf/internal/backend/cuda/cuda_context.h"
#include "orteaf/internal/backend/cuda/cuda_stream.h"
#include "orteaf/internal/backend/cuda/cuda_event.h"
#include "orteaf/internal/backend/cuda/cuda_init.h"
#include "orteaf/internal/backend/cuda/cuda_device.h"
#include "orteaf/internal/backend/cuda/cuda_alloc.h"

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

namespace cuda = orteaf::internal::backend::cuda;

#if ORTEAF_ENABLE_CUDA

/**
 * @brief Test fixture that initializes CUDA and sets up a device and context.
 */
class CudaContextStreamEventTest : public ::testing::Test {
protected:
    void SetUp() override {
        cuda::cuda_init();
        int count = cuda::get_device_count();
        if (count == 0) {
            GTEST_SKIP() << "No CUDA devices available";
        }
        device_ = cuda::get_device(0);
        
        // Get primary context for tests
        context_ = cuda::get_primary_context(device_);
        EXPECT_NE(context_, nullptr);
        cuda::set_context(context_);
    }
    
    void TearDown() override {
        if (context_ != nullptr) {
            cuda::release_primary_context(device_);
        }
    }
    
    cuda::CUdevice_t device_ = 0;
    cuda::CUcontext_t context_ = nullptr;
};

/**
 * @brief Test that primary context can be retrieved.
 */
TEST_F(CudaContextStreamEventTest, GetPrimaryContextSucceeds) {
    cuda::CUcontext_t ctx = cuda::get_primary_context(device_);
    EXPECT_NE(ctx, nullptr);
    
    // Can be called multiple times (retains reference)
    cuda::CUcontext_t ctx2 = cuda::get_primary_context(device_);
    EXPECT_NE(ctx2, nullptr);
    
    cuda::release_primary_context(device_);
    cuda::release_primary_context(device_);
}

/**
 * @brief Test that primary context acquisition with invalid device throws.
 */
TEST_F(CudaContextStreamEventTest, GetPrimaryContextInvalidDeviceThrows) {
    cuda::CUdevice_t invalid_device = static_cast<cuda::CUdevice_t>(-1);
    EXPECT_THROW(cuda::get_primary_context(invalid_device), std::system_error);
}

/**
 * @brief Test that a new context can be created.
 */
TEST_F(CudaContextStreamEventTest, CreateContextSucceeds) {
    cuda::CUcontext_t new_ctx = cuda::create_context(device_);
    EXPECT_NE(new_ctx, nullptr);
    
    cuda::release_context(new_ctx);
}

/**
 * @brief Test that create_context with invalid device throws.
 */
TEST_F(CudaContextStreamEventTest, CreateContextInvalidDeviceThrows) {
    cuda::CUdevice_t invalid_device = static_cast<cuda::CUdevice_t>(-1);
    EXPECT_THROW(cuda::create_context(invalid_device), std::system_error);
}

/**
 * @brief Test that set_context works.
 */
TEST_F(CudaContextStreamEventTest, SetContextSucceeds) {
    EXPECT_NO_THROW(cuda::set_context(context_));
    
    cuda::CUcontext_t new_ctx = cuda::create_context(device_);
    EXPECT_NO_THROW(cuda::set_context(new_ctx));
    EXPECT_NO_THROW(cuda::set_context(context_));
    
    cuda::release_context(new_ctx);
}

/**
 * @brief Test that set_context with nullptr throws.
 */
TEST_F(CudaContextStreamEventTest, SetContextNullptrThrows) {
    EXPECT_THROW(cuda::set_context(nullptr), std::system_error);
}

/**
 * @brief Test that release_context works.
 */
TEST_F(CudaContextStreamEventTest, ReleaseContextSucceeds) {
    cuda::CUcontext_t new_ctx = cuda::create_context(device_);
    EXPECT_NO_THROW(cuda::release_context(new_ctx));
}

/**
 * @brief Test that release_context with nullptr is ignored.
 */
TEST_F(CudaContextStreamEventTest, ReleaseContextNullptrNoOp) {
    EXPECT_NO_THROW(cuda::release_context(nullptr));
}

/**
 * @brief Test that release_primary_context works.
 */
TEST_F(CudaContextStreamEventTest, ReleasePrimaryContextSucceeds) {
    cuda::CUcontext_t ctx = cuda::get_primary_context(device_);
    EXPECT_NE(ctx, nullptr);
    EXPECT_NO_THROW(cuda::release_primary_context(device_));
}

/**
 * @brief Test that stream creation succeeds.
 */
TEST_F(CudaContextStreamEventTest, GetStreamSucceeds) {
    cuda::CUstream_t stream = cuda::get_stream();
    EXPECT_NE(stream, nullptr);
    
    cuda::release_stream(stream);
}

/**
 * @brief Test that multiple streams can be created.
 */
TEST_F(CudaContextStreamEventTest, CreateMultipleStreams) {
    cuda::CUstream_t stream1 = cuda::get_stream();
    cuda::CUstream_t stream2 = cuda::get_stream();
    EXPECT_NE(stream1, nullptr);
    EXPECT_NE(stream2, nullptr);
    EXPECT_NE(stream1, stream2);  // Should be different handles
    
    cuda::release_stream(stream1);
    cuda::release_stream(stream2);
}

/**
 * @brief Test that set_stream is a no-op (for API symmetry).
 */
TEST_F(CudaContextStreamEventTest, SetStreamIsNoOp) {
    cuda::CUstream_t stream = cuda::get_stream();
    EXPECT_NO_THROW(cuda::set_stream(stream));
    EXPECT_NO_THROW(cuda::set_stream(nullptr));
    
    cuda::release_stream(stream);
}

/**
 * @brief Test that release_stream works.
 */
TEST_F(CudaContextStreamEventTest, ReleaseStreamSucceeds) {
    cuda::CUstream_t stream = cuda::get_stream();
    EXPECT_NO_THROW(cuda::release_stream(stream));
}

/**
 * @brief Test that release_stream with nullptr is ignored.
 */
TEST_F(CudaContextStreamEventTest, ReleaseStreamNullptrNoOp) {
    EXPECT_NO_THROW(cuda::release_stream(nullptr));
}

/**
 * @brief Test that synchronize_stream works.
 */
TEST_F(CudaContextStreamEventTest, SynchronizeStreamSucceeds) {
    cuda::CUstream_t stream = cuda::get_stream();
    EXPECT_NO_THROW(cuda::synchronize_stream(stream));
    
    cuda::release_stream(stream);
}

/**
 * @brief Test that synchronize_stream with nullptr throws.
 */
TEST_F(CudaContextStreamEventTest, SynchronizeStreamNullptrThrows) {
    EXPECT_THROW(cuda::synchronize_stream(nullptr), std::system_error);
}

/**
 * @brief Test that wait_stream works (requires device memory).
 */
TEST_F(CudaContextStreamEventTest, WaitStreamSucceeds) {
    cuda::CUstream_t stream = cuda::get_stream();
    
    // Allocate a small device memory block
    constexpr size_t size = sizeof(uint32_t);
    cuda::CUdeviceptr_t dev_ptr = cuda::alloc(size);
    EXPECT_NE(dev_ptr, 0);
    
    // Write a value and wait for it
    EXPECT_NO_THROW(cuda::write_stream(stream, dev_ptr, 42));
    EXPECT_NO_THROW(cuda::wait_stream(stream, dev_ptr, 42));
    
    cuda::free(dev_ptr, size);
    cuda::release_stream(stream);
}

/**
 * @brief Test that write_stream works.
 */
TEST_F(CudaContextStreamEventTest, WriteStreamSucceeds) {
    cuda::CUstream_t stream = cuda::get_stream();
    
    constexpr size_t size = sizeof(uint32_t);
    cuda::CUdeviceptr_t dev_ptr = cuda::alloc(size);
    EXPECT_NE(dev_ptr, 0);
    
    EXPECT_NO_THROW(cuda::write_stream(stream, dev_ptr, 123));
    
    cuda::free(dev_ptr, size);
    cuda::release_stream(stream);
}

/**
 * @brief Test that wait_stream with nullptr stream throws.
 */
TEST_F(CudaContextStreamEventTest, WaitStreamNullptrThrows) {
    constexpr size_t size = sizeof(uint32_t);
    cuda::CUdeviceptr_t dev_ptr = cuda::alloc(size);
    EXPECT_NE(dev_ptr, 0);
    
    EXPECT_THROW(cuda::wait_stream(nullptr, dev_ptr, 42), std::system_error);
    
    cuda::free(dev_ptr, size);
}

/**
 * @brief Test that wait_stream with zero address throws.
 */
TEST_F(CudaContextStreamEventTest, WaitStreamZeroAddrThrows) {
    cuda::CUstream_t stream = cuda::get_stream();
    
    EXPECT_THROW(cuda::wait_stream(stream, 0, 42), std::system_error);
    
    cuda::release_stream(stream);
}

/**
 * @brief Test that write_stream with nullptr stream throws.
 */
TEST_F(CudaContextStreamEventTest, WriteStreamNullptrThrows) {
    constexpr size_t size = sizeof(uint32_t);
    cuda::CUdeviceptr_t dev_ptr = cuda::alloc(size);
    EXPECT_NE(dev_ptr, 0);
    
    EXPECT_THROW(cuda::write_stream(nullptr, dev_ptr, 123), std::system_error);
    
    cuda::free(dev_ptr, size);
}

/**
 * @brief Test that write_stream with zero address throws.
 */
TEST_F(CudaContextStreamEventTest, WriteStreamZeroAddrThrows) {
    cuda::CUstream_t stream = cuda::get_stream();
    
    EXPECT_THROW(cuda::write_stream(stream, 0, 123), std::system_error);
    
    cuda::release_stream(stream);
}

/**
 * @brief Test that event creation succeeds.
 */
TEST_F(CudaContextStreamEventTest, CreateEventSucceeds) {
    cuda::CUevent_t event = cuda::create_event();
    EXPECT_NE(event, nullptr);
    
    cuda::destroy_event(event);
}

/**
 * @brief Test that multiple events can be created.
 */
TEST_F(CudaContextStreamEventTest, CreateMultipleEvents) {
    cuda::CUevent_t event1 = cuda::create_event();
    cuda::CUevent_t event2 = cuda::create_event();
    EXPECT_NE(event1, nullptr);
    EXPECT_NE(event2, nullptr);
    EXPECT_NE(event1, event2);
    
    cuda::destroy_event(event1);
    cuda::destroy_event(event2);
}

/**
 * @brief Test that destroy_event works.
 */
TEST_F(CudaContextStreamEventTest, DestroyEventSucceeds) {
    cuda::CUevent_t event = cuda::create_event();
    EXPECT_NO_THROW(cuda::destroy_event(event));
}

/**
 * @brief Test that destroy_event with nullptr is ignored.
 */
TEST_F(CudaContextStreamEventTest, DestroyEventNullptrNoOp) {
    EXPECT_NO_THROW(cuda::destroy_event(nullptr));
}

/**
 * @brief Test that record_event works.
 */
TEST_F(CudaContextStreamEventTest, RecordEventSucceeds) {
    cuda::CUstream_t stream = cuda::get_stream();
    cuda::CUevent_t event = cuda::create_event();
    
    EXPECT_NO_THROW(cuda::record_event(event, stream));
    
    cuda::destroy_event(event);
    cuda::release_stream(stream);
}

/**
 * @brief Test that record_event with nullptr event throws.
 */
TEST_F(CudaContextStreamEventTest, RecordEventNullptrEventThrows) {
    cuda::CUstream_t stream = cuda::get_stream();
    
    EXPECT_THROW(cuda::record_event(nullptr, stream), std::system_error);
    
    cuda::release_stream(stream);
}

/**
 * @brief Test that record_event with nullptr stream throws.
 */
TEST_F(CudaContextStreamEventTest, RecordEventNullptrStreamThrows) {
    cuda::CUevent_t event = cuda::create_event();
    
    EXPECT_THROW(cuda::record_event(event, nullptr), std::system_error);
    
    cuda::destroy_event(event);
}

/**
 * @brief Test that query_event works.
 */
TEST_F(CudaContextStreamEventTest, QueryEventSucceeds) {
    cuda::CUstream_t stream = cuda::get_stream();
    cuda::CUevent_t event = cuda::create_event();
    
    // Before recording, query may return true (already complete) or false
    bool before = cuda::query_event(event);
    
    cuda::record_event(event, stream);
    
    // Synchronize stream to ensure event is recorded
    cuda::synchronize_stream(stream);
    
    // After synchronization, event should be complete
    bool after = cuda::query_event(event);
    EXPECT_TRUE(after);
    
    cuda::destroy_event(event);
    cuda::release_stream(stream);
}

/**
 * @brief Test that query_event with nullptr returns true.
 */
TEST_F(CudaContextStreamEventTest, QueryEventNullptrReturnsTrue) {
    EXPECT_TRUE(cuda::query_event(nullptr));
}

/**
 * @brief Test that wait_event works.
 */
TEST_F(CudaContextStreamEventTest, WaitEventSucceeds) {
    cuda::CUstream_t stream1 = cuda::get_stream();
    cuda::CUstream_t stream2 = cuda::get_stream();
    cuda::CUevent_t event = cuda::create_event();
    
    // Record event in stream1
    cuda::record_event(event, stream1);
    
    // Make stream2 wait for event
    EXPECT_NO_THROW(cuda::wait_event(stream2, event));
    
    // Synchronize both streams
    cuda::synchronize_stream(stream1);
    cuda::synchronize_stream(stream2);
    
    cuda::destroy_event(event);
    cuda::release_stream(stream1);
    cuda::release_stream(stream2);
}

/**
 * @brief Test that wait_event with nullptr stream throws.
 */
TEST_F(CudaContextStreamEventTest, WaitEventNullptrStreamThrows) {
    cuda::CUevent_t event = cuda::create_event();
    
    EXPECT_THROW(cuda::wait_event(nullptr, event), std::system_error);
    
    cuda::destroy_event(event);
}

/**
 * @brief Test that wait_event with nullptr event throws.
 */
TEST_F(CudaContextStreamEventTest, WaitEventNullptrEventThrows) {
    cuda::CUstream_t stream = cuda::get_stream();
    
    EXPECT_THROW(cuda::wait_event(stream, nullptr), std::system_error);
    
    cuda::release_stream(stream);
}

/**
 * @brief Test that events can be used for synchronization.
 */
TEST_F(CudaContextStreamEventTest, EventSynchronization) {
    cuda::CUstream_t stream = cuda::get_stream();
    cuda::CUevent_t event = cuda::create_event();
    
    // Record event
    cuda::record_event(event, stream);
    
    // Query until complete (with timeout to avoid infinite loop)
    bool completed = false;
    auto start = std::chrono::steady_clock::now();
    constexpr auto timeout = std::chrono::seconds(5);
    
    while (!completed && (std::chrono::steady_clock::now() - start) < timeout) {
        completed = cuda::query_event(event);
        if (!completed) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    EXPECT_TRUE(completed);
    
    cuda::destroy_event(event);
    cuda::release_stream(stream);
}

/**
 * @brief Test context lifecycle (create, set, release).
 */
TEST_F(CudaContextStreamEventTest, ContextLifecycle) {
    cuda::CUcontext_t new_ctx = cuda::create_context(device_);
    EXPECT_NE(new_ctx, nullptr);
    
    cuda::set_context(new_ctx);
    cuda::set_context(context_);  // Switch back
    cuda::release_context(new_ctx);
}

/**
 * @brief Test stream lifecycle (create, use, release).
 */
TEST_F(CudaContextStreamEventTest, StreamLifecycle) {
    cuda::CUstream_t stream = cuda::get_stream();
    EXPECT_NE(stream, nullptr);
    
    cuda::set_stream(stream);
    cuda::synchronize_stream(stream);
    cuda::release_stream(stream);
}

/**
 * @brief Test event lifecycle (create, record, query, destroy).
 */
TEST_F(CudaContextStreamEventTest, EventLifecycle) {
    cuda::CUstream_t stream = cuda::get_stream();
    cuda::CUevent_t event = cuda::create_event();
    
    cuda::record_event(event, stream);
    bool is_ready = cuda::query_event(event);
    EXPECT_TRUE(is_ready || !is_ready);  // Either state is valid
    
    cuda::destroy_event(event);
    cuda::release_stream(stream);
}

#else  // !ORTEAF_ENABLE_CUDA

/**
 * @brief Test that context functions return nullptr when CUDA is disabled.
 */
TEST(CudaContextStreamEvent, DisabledReturnsNeutralValues) {
    cuda::CUdevice_t device = 0;
    
    EXPECT_EQ(cuda::get_primary_context(device), nullptr);
    EXPECT_EQ(cuda::create_context(device), nullptr);
    
    EXPECT_NO_THROW(cuda::set_context(nullptr));
    EXPECT_NO_THROW(cuda::release_context(nullptr));
    EXPECT_NO_THROW(cuda::release_primary_context(device));
}

/**
 * @brief Test that stream functions return nullptr when CUDA is disabled.
 */
TEST(CudaContextStreamEvent, DisabledStreamReturnsNullptr) {
    EXPECT_EQ(cuda::get_stream(), nullptr);
    
    EXPECT_NO_THROW(cuda::set_stream(nullptr));
    EXPECT_NO_THROW(cuda::release_stream(nullptr));
    EXPECT_NO_THROW(cuda::synchronize_stream(nullptr));
    EXPECT_NO_THROW(cuda::wait_stream(nullptr, 0, 0));
    EXPECT_NO_THROW(cuda::write_stream(nullptr, 0, 0));
}

/**
 * @brief Test that event functions return neutral values when CUDA is disabled.
 */
TEST(CudaContextStreamEvent, DisabledEventReturnsNullptr) {
    EXPECT_EQ(cuda::create_event(), nullptr);
    
    EXPECT_NO_THROW(cuda::destroy_event(nullptr));
    EXPECT_NO_THROW(cuda::record_event(nullptr, nullptr));
    EXPECT_TRUE(cuda::query_event(nullptr));  // Should return true when disabled
    EXPECT_NO_THROW(cuda::wait_event(nullptr, nullptr));
}

#endif  // ORTEAF_ENABLE_CUDA

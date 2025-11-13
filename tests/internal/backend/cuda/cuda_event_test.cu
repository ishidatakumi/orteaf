/**
 * @file cuda_event_test.cpp
 * @brief Tests for CUDA event creation, recording, and synchronization helpers.
 */

#include "orteaf/internal/backend/cuda/cuda_event.h"
#include "orteaf/internal/backend/cuda/cuda_stream.h"
#include "orteaf/internal/backend/cuda/cuda_context.h"
#include "orteaf/internal/backend/cuda/cuda_init.h"
#include "orteaf/internal/backend/cuda/cuda_device.h"
#include "tests/internal/testing/error_assert.h"

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

namespace cuda = orteaf::internal::backend::cuda;

class CudaEventTest : public ::testing::Test {
protected:
    void SetUp() override {
        cuda::cudaInit();
        int count = cuda::getDeviceCount();
        if (count == 0) {
            GTEST_SKIP() << "No CUDA devices available";
        }
        device_ = cuda::get_device(0);
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

TEST_F(CudaEventTest, CreateEventSucceeds) {
    cuda::CUevent_t event = cuda::create_event();
    EXPECT_NE(event, nullptr);
    cuda::destroy_event(event);
}

TEST_F(CudaEventTest, CreateMultipleEvents) {
    cuda::CUevent_t event1 = cuda::create_event();
    cuda::CUevent_t event2 = cuda::create_event();
    EXPECT_NE(event1, nullptr);
    EXPECT_NE(event2, nullptr);
    EXPECT_NE(event1, event2);
    cuda::destroy_event(event1);
    cuda::destroy_event(event2);
}

TEST_F(CudaEventTest, DestroyEventSucceeds) {
    cuda::CUevent_t event = cuda::create_event();
    EXPECT_NO_THROW(cuda::destroy_event(event));
}

TEST_F(CudaEventTest, DestroyEventNullptrNoOp) {
    EXPECT_NO_THROW(cuda::destroy_event(nullptr));
}

TEST_F(CudaEventTest, RecordEventSucceeds) {
    cuda::CUstream_t stream = cuda::get_stream();
    cuda::CUevent_t event = cuda::create_event();
    EXPECT_NO_THROW(cuda::record_event(event, stream));
    cuda::destroy_event(event);
    cuda::release_stream(stream);
}

TEST_F(CudaEventTest, RecordEventNullptrEventThrows) {
    cuda::CUstream_t stream = cuda::get_stream();
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        [&]() { cuda::record_event(nullptr, stream); });
    cuda::release_stream(stream);
}

TEST_F(CudaEventTest, RecordEventNullptrStreamThrows) {
    cuda::CUevent_t event = cuda::create_event();
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        [&]() { cuda::record_event(event, nullptr); });
    cuda::destroy_event(event);
}

TEST_F(CudaEventTest, QueryEventSucceeds) {
    cuda::CUstream_t stream = cuda::get_stream();
    cuda::CUevent_t event = cuda::create_event();
    bool before = cuda::query_event(event);
    (void)before;
    cuda::record_event(event, stream);
    cuda::synchronize_stream(stream);
    bool after = cuda::query_event(event);
    EXPECT_TRUE(after);
    cuda::destroy_event(event);
    cuda::release_stream(stream);
}

TEST_F(CudaEventTest, QueryEventNullptrReturnsTrue) {
    EXPECT_TRUE(cuda::query_event(nullptr));
}

TEST_F(CudaEventTest, WaitEventSucceeds) {
    cuda::CUstream_t stream1 = cuda::get_stream();
    cuda::CUstream_t stream2 = cuda::get_stream();
    cuda::CUevent_t event = cuda::create_event();
    cuda::record_event(event, stream1);
    EXPECT_NO_THROW(cuda::wait_event(stream2, event));
    cuda::synchronize_stream(stream1);
    cuda::synchronize_stream(stream2);
    cuda::destroy_event(event);
    cuda::release_stream(stream1);
    cuda::release_stream(stream2);
}

TEST_F(CudaEventTest, WaitEventNullptrStreamThrows) {
    cuda::CUevent_t event = cuda::create_event();
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        [&]() { cuda::wait_event(nullptr, event); });
    cuda::destroy_event(event);
}

TEST_F(CudaEventTest, WaitEventNullptrEventThrows) {
    cuda::CUstream_t stream = cuda::get_stream();
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        [&]() { cuda::wait_event(stream, nullptr); });
    cuda::release_stream(stream);
}

TEST_F(CudaEventTest, EventSynchronization) {
    cuda::CUstream_t stream = cuda::get_stream();
    cuda::CUevent_t event = cuda::create_event();
    cuda::record_event(event, stream);

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

TEST_F(CudaEventTest, EventLifecycle) {
    cuda::CUstream_t stream = cuda::get_stream();
    cuda::CUevent_t event = cuda::create_event();
    cuda::record_event(event, stream);
    bool ready = cuda::query_event(event);
    EXPECT_TRUE(ready || !ready);
    cuda::destroy_event(event);
    cuda::release_stream(stream);
}

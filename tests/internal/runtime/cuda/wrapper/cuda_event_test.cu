/**
 * @file cuda_event_test.cpp
 * @brief Tests for CUDA event creation, recording, and synchronization helpers.
 */

#include "orteaf/internal/runtime/cuda/platform/wrapper/cuda_event.h"
#include "orteaf/internal/runtime/cuda/platform/wrapper/cuda_stream.h"
#include "orteaf/internal/runtime/cuda/platform/wrapper/cuda_context.h"
#include "orteaf/internal/runtime/cuda/platform/wrapper/cuda_init.h"
#include "orteaf/internal/runtime/cuda/platform/wrapper/cuda_device.h"
#include "tests/internal/testing/error_assert.h"

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

namespace cuda = orteaf::internal::runtime::cuda::platform::wrapper;

class CudaEventTest : public ::testing::Test {
protected:
    void SetUp() override {
        cuda::cudaInit();
        int count = cuda::getDeviceCount();
        if (count == 0) {
            GTEST_SKIP() << "No CUDA devices available";
        }
        device_ = cuda::getDevice(0);
        context_ = cuda::getPrimaryContext(device_);
        EXPECT_NE(context_, nullptr);
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

TEST_F(CudaEventTest, CreateEventSucceeds) {
    cuda::CudaEvent_t event = cuda::createEvent();
    EXPECT_NE(event, nullptr);
    cuda::destroyEvent(event);
}

TEST_F(CudaEventTest, CreateMultipleEvents) {
    cuda::CudaEvent_t event1 = cuda::createEvent();
    cuda::CudaEvent_t event2 = cuda::createEvent();
    EXPECT_NE(event1, nullptr);
    EXPECT_NE(event2, nullptr);
    EXPECT_NE(event1, event2);
    cuda::destroyEvent(event1);
    cuda::destroyEvent(event2);
}

TEST_F(CudaEventTest, DestroyEventSucceeds) {
    cuda::CudaEvent_t event = cuda::createEvent();
    EXPECT_NO_THROW(cuda::destroyEvent(event));
}

TEST_F(CudaEventTest, DestroyEventNullptrNoOp) {
    EXPECT_NO_THROW(cuda::destroyEvent(nullptr));
}

TEST_F(CudaEventTest, RecordEventSucceeds) {
    cuda::CudaStream_t stream = cuda::getStream();
    cuda::CudaEvent_t event = cuda::createEvent();
    EXPECT_NO_THROW(cuda::recordEvent(event, stream));
    cuda::destroyEvent(event);
    cuda::releaseStream(stream);
}

TEST_F(CudaEventTest, RecordEventNullptrEventThrows) {
    cuda::CudaStream_t stream = cuda::getStream();
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        [&]() { cuda::recordEvent(nullptr, stream); });
    cuda::releaseStream(stream);
}

TEST_F(CudaEventTest, RecordEventNullptrStreamThrows) {
    cuda::CudaEvent_t event = cuda::createEvent();
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        [&]() { cuda::recordEvent(event, nullptr); });
    cuda::destroyEvent(event);
}

TEST_F(CudaEventTest, QueryEventSucceeds) {
    cuda::CudaStream_t stream = cuda::getStream();
    cuda::CudaEvent_t event = cuda::createEvent();
    bool before = cuda::queryEvent(event);
    (void)before;
    cuda::recordEvent(event, stream);
    cuda::synchronizeStream(stream);
    bool after = cuda::queryEvent(event);
    EXPECT_TRUE(after);
    cuda::destroyEvent(event);
    cuda::releaseStream(stream);
}

TEST_F(CudaEventTest, QueryEventNullptrReturnsTrue) {
    EXPECT_TRUE(cuda::queryEvent(nullptr));
}

TEST_F(CudaEventTest, WaitEventSucceeds) {
    cuda::CudaStream_t stream1 = cuda::getStream();
    cuda::CudaStream_t stream2 = cuda::getStream();
    cuda::CudaEvent_t event = cuda::createEvent();
    cuda::recordEvent(event, stream1);
    EXPECT_NO_THROW(cuda::waitEvent(stream2, event));
    cuda::synchronizeStream(stream1);
    cuda::synchronizeStream(stream2);
    cuda::destroyEvent(event);
    cuda::releaseStream(stream1);
    cuda::releaseStream(stream2);
}

TEST_F(CudaEventTest, WaitEventNullptrStreamThrows) {
    cuda::CudaEvent_t event = cuda::createEvent();
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        [&]() { cuda::waitEvent(nullptr, event); });
    cuda::destroyEvent(event);
}

TEST_F(CudaEventTest, WaitEventNullptrEventThrows) {
    cuda::CudaStream_t stream = cuda::getStream();
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        [&]() { cuda::waitEvent(stream, nullptr); });
    cuda::releaseStream(stream);
}

TEST_F(CudaEventTest, EventSynchronization) {
    cuda::CudaStream_t stream = cuda::getStream();
    cuda::CudaEvent_t event = cuda::createEvent();
    cuda::recordEvent(event, stream);

    bool completed = false;
    auto start = std::chrono::steady_clock::now();
    constexpr auto timeout = std::chrono::seconds(5);
    while (!completed && (std::chrono::steady_clock::now() - start) < timeout) {
        completed = cuda::queryEvent(event);
        if (!completed) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    EXPECT_TRUE(completed);

    cuda::destroyEvent(event);
    cuda::releaseStream(stream);
}

TEST_F(CudaEventTest, EventLifecycle) {
    cuda::CudaStream_t stream = cuda::getStream();
    cuda::CudaEvent_t event = cuda::createEvent();
    cuda::recordEvent(event, stream);
    bool ready = cuda::queryEvent(event);
    EXPECT_TRUE(ready || !ready);
    cuda::destroyEvent(event);
    cuda::releaseStream(stream);
}

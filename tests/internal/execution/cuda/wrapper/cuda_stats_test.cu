/**
 * @file cuda_stats_test.cpp
 * @brief Tests for CUDA statistics tracking.
 */

#include "orteaf/internal/execution/cuda/platform/wrapper/cuda_stats.h"
#include "orteaf/internal/execution/cuda/platform/wrapper/cuda_init.h"
#include "orteaf/internal/execution/cuda/platform/wrapper/cuda_device.h"
#include "orteaf/internal/execution/cuda/platform/wrapper/cuda_context.h"
#include "orteaf/internal/execution/cuda/platform/wrapper/cuda_stream.h"
#include "orteaf/internal/execution/cuda/platform/wrapper/cuda_event.h"

#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace cuda = orteaf::internal::execution::cuda::platform::wrapper;

#if defined(ORTEAF_STATS_LEVEL_CUDA_VALUE)

/**
 * @brief Test fixture that initializes CUDA and resets stats.
 */
class CudaStatsTest : public ::testing::Test {
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

#if ORTEAF_STATS_LEVEL_CUDA_VALUE <= 2

/**
 * @brief Test that update_alloc increments counters.
 */
TEST_F(CudaStatsTest, UpdateAllocIncrementsCounters) {
    auto& stats = cuda::statsInstance();
    uint64_t initial = stats.totalAllocations();
    
    cuda::updateAlloc(1024);
    
    EXPECT_EQ(stats.totalAllocations(), initial + 1);
    EXPECT_EQ(stats.activeAllocations(), 1);
}

/**
 * @brief Test that update_dealloc decrements active allocations.
 */
TEST_F(CudaStatsTest, UpdateDeallocDecrementsActive) {
    auto& stats = cuda::statsInstance();
    
    cuda::updateAlloc(1024);
    EXPECT_EQ(stats.activeAllocations(), 1);
    EXPECT_EQ(stats.totalDeallocations(), 0);
    
    cuda::updateDealloc(1024);
    EXPECT_EQ(stats.activeAllocations(), 0);
    EXPECT_EQ(stats.totalDeallocations(), 1);
}

/**
 * @brief Test that multiple allocations are tracked correctly.
 */
TEST_F(CudaStatsTest, MultipleAllocationsTracked) {
    auto& stats = cuda::statsInstance();
    uint64_t initial_total = stats.totalAllocations();
    uint64_t initial_active = stats.activeAllocations();
    
    cuda::updateAlloc(1024);
    cuda::updateAlloc(2048);
    cuda::updateAlloc(4096);
    
    EXPECT_EQ(stats.totalAllocations(), initial_total + 3);
    EXPECT_EQ(stats.activeAllocations(), initial_active + 3);
    
    cuda::updateDealloc(1024);
    EXPECT_EQ(stats.activeAllocations(), initial_active + 2);
    
    cuda::updateDealloc(2048);
    cuda::updateDealloc(4096);
    EXPECT_EQ(stats.activeAllocations(), initial_active);
}

/**
 * @brief Test that updateDeviceSwitch increments counter.
 */
TEST_F(CudaStatsTest, UpdateDeviceSwitchIncrements) {
    auto& stats = cuda::statsInstance();
    uint64_t initial = stats.deviceSwitches();
    
    cuda::updateDeviceSwitch();
    EXPECT_EQ(stats.deviceSwitches(), initial + 1);
    
    cuda::updateDeviceSwitch();
    EXPECT_EQ(stats.deviceSwitches(), initial + 2);
}

/**
 * @brief Test that statistics are thread-safe.
 */
TEST_F(CudaStatsTest, StatisticsAreThreadSafe) {
    auto& stats = cuda::statsInstance();
    constexpr int num_threads = 4;
    constexpr int ops_per_thread = 100;
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&stats]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                cuda::updateAlloc(1024);
                cuda::updateDealloc(1024);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // All allocations should be deallocated
    EXPECT_EQ(stats.activeAllocations(), 0);
    EXPECT_EQ(stats.totalAllocations(), num_threads * ops_per_thread);
    EXPECT_EQ(stats.totalDeallocations(), num_threads * ops_per_thread);
}

#endif  // ORTEAF_STATS_LEVEL_CUDA_VALUE <= 2

#if ORTEAF_STATS_LEVEL_CUDA_VALUE <= 4

/**
 * @brief Test that current_allocated_bytes tracks correctly.
 */
TEST_F(CudaStatsTest, CurrentAllocatedBytesTracked) {
    auto& stats = cuda::statsInstance();
    uint64_t initial = stats.currentAllocatedBytes();
    
    cuda::updateAlloc(1024);
    EXPECT_EQ(stats.currentAllocatedBytes(), initial + 1024);
    
    cuda::updateAlloc(2048);
    EXPECT_EQ(stats.currentAllocatedBytes(), initial + 1024 + 2048);
    
    cuda::updateDealloc(1024);
    EXPECT_EQ(stats.currentAllocatedBytes(), initial + 2048);
}

/**
 * @brief Test that peakAllocatedBytes tracks maximum.
 */
TEST_F(CudaStatsTest, PeakAllocatedBytesTracksMaximum) {
    auto& stats = cuda::statsInstance();
    uint64_t initial_peak = stats.peakAllocatedBytes();
    uint64_t initial_current = stats.currentAllocatedBytes();
    
    cuda::updateAlloc(1024);
    uint64_t peak1 = stats.peakAllocatedBytes();
    EXPECT_GE(peak1, initial_peak);
    
    cuda::updateAlloc(2048);
    uint64_t peak2 = stats.peakAllocatedBytes();
    EXPECT_GE(peak2, peak1);
    
    cuda::updateDealloc(1024);
    uint64_t peak3 = stats.peakAllocatedBytes();
    EXPECT_EQ(peak3, peak2);  // Peak should not decrease
    
    cuda::updateDealloc(2048);
    EXPECT_EQ(stats.currentAllocatedBytes(), initial_current);
    EXPECT_EQ(stats.peakAllocatedBytes(), peak2);  // Peak should remain
}

/**
 * @brief Test that updateCreateEvent increments active_events.
 */
TEST_F(CudaStatsTest, UpdateCreateEventIncrements) {
    auto& stats = cuda::statsInstance();
    uint64_t initial = stats.activeEvents();
    
    cuda::updateCreateEvent();
    EXPECT_EQ(stats.activeEvents(), initial + 1);
    
    cuda::updateCreateEvent();
    EXPECT_EQ(stats.activeEvents(), initial + 2);
}

/**
 * @brief Test that updateDestroyEvent decrements active_events.
 */
TEST_F(CudaStatsTest, UpdateDestroyEventDecrements) {
    auto& stats = cuda::statsInstance();
    
    cuda::updateCreateEvent();
    cuda::updateCreateEvent();
    EXPECT_EQ(stats.activeEvents(), 2);
    
    cuda::updateDestroyEvent();
    EXPECT_EQ(stats.activeEvents(), 1);
    
    cuda::updateDestroyEvent();
    EXPECT_EQ(stats.activeEvents(), 0);
}

/**
 * @brief Test that update_create_stream increments activeStreams.
 */
TEST_F(CudaStatsTest, UpdateCreateStreamIncrements) {
    auto& stats = cuda::statsInstance();
    uint64_t initial = stats.activeStreams();
    
    cuda::updateCreateStream();
    EXPECT_EQ(stats.activeStreams(), initial + 1);
    
    cuda::updateCreateStream();
    EXPECT_EQ(stats.activeStreams(), initial + 2);
}

/**
 * @brief Test that update_destroy_stream decrements activeStreams.
 */
TEST_F(CudaStatsTest, UpdateDestroyStreamDecrements) {
    auto& stats = cuda::statsInstance();
    
    cuda::updateCreateStream();
    cuda::updateCreateStream();
    EXPECT_EQ(stats.activeStreams(), 2);
    
    cuda::updateDestroyStream();
    EXPECT_EQ(stats.activeStreams(), 1);
    
    cuda::updateDestroyStream();
    EXPECT_EQ(stats.activeStreams(), 0);
}

/**
 * @brief Test that updateActiveEvent increments activeEvents.
 */
TEST_F(CudaStatsTest, UpdateActiveEventIncrements) {
    auto& stats = cuda::statsInstance();
    uint64_t initial = stats.activeEvents();
    
    cuda::updateActiveEvent();
    EXPECT_EQ(stats.activeEvents(), initial + 1);
    
    cuda::updateActiveEvent();
    EXPECT_EQ(stats.activeEvents(), initial + 2);
}

/**
 * @brief Test that statistics are updated when creating/destroying streams.
 */
TEST_F(CudaStatsTest, StreamCreationUpdatesStats) {
    auto& stats = cuda::statsInstance();
    uint64_t initial_streams = stats.activeStreams();
    
    cuda::CudaStream_t stream1 = cuda::getStream();
    EXPECT_EQ(stats.activeStreams(), initial_streams + 1);
    
    cuda::CudaStream_t stream2 = cuda::getStream();
    EXPECT_EQ(stats.activeStreams(), initial_streams + 2);
    
    cuda::releaseStream(stream1);
    EXPECT_EQ(stats.activeStreams(), initial_streams + 1);
    
    cuda::releaseStream(stream2);
    EXPECT_EQ(stats.activeStreams(), initial_streams);
}

/**
 * @brief Test that statistics are updated when creating/destroying events.
 */
TEST_F(CudaStatsTest, EventCreationUpdatesStats) {
    auto& stats = cuda::statsInstance();
    uint64_t initial_events = stats.activeEvents();
    
    cuda::CudaEvent_t event1 = cuda::createEvent();
    EXPECT_EQ(stats.activeEvents(), initial_events + 1);
    
    cuda::CudaEvent_t event2 = cuda::createEvent();
    EXPECT_EQ(stats.activeEvents(), initial_events + 2);
    
    cuda::destroyEvent(event1);
    EXPECT_EQ(stats.activeEvents(), initial_events + 1);
    
    cuda::destroyEvent(event2);
    EXPECT_EQ(stats.activeEvents(), initial_events);
}

#endif  // ORTEAF_STATS_LEVEL_CUDA_VALUE <= 4

/**
 * @brief Test that toString produces valid output.
 */
TEST_F(CudaStatsTest, ToStringProducesValidOutput) {
    auto& stats = cuda::statsInstance();
    std::string str = stats.toString();
    
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("CUDA Stats"), std::string::npos);
}

/**
 * @brief Test that toString is not empty even with no operations.
 */
TEST_F(CudaStatsTest, ToStringNotEmptyWhenEmpty) {
    auto& stats = cuda::statsInstance();
    std::string str = stats.toString();
    
    EXPECT_FALSE(str.empty());
}

/**
 * @brief Test that stream output operator works.
 */
TEST_F(CudaStatsTest, StreamOutputOperatorWorks) {
    auto& stats = cuda::statsInstance();
    std::ostringstream oss;
    
    oss << stats;
    
    EXPECT_FALSE(oss.str().empty());
}

/**
 * @brief Test that statsInstance returns singleton.
 */
TEST_F(CudaStatsTest, StatsInstanceIsSingleton) {
    cuda::CudaStats& stats1 = cuda::statsInstance();
    cuda::CudaStats& stats2 = cuda::statsInstance();
    
    EXPECT_EQ(&stats1, &stats2);
}

/**
 * @brief Test that cuda_stats returns same instance.
 */
TEST_F(CudaStatsTest, CudaStatsReturnsSameInstance) {
    cuda::CudaStats& stats1 = cuda::statsInstance();
    cuda::CudaStats& stats2 = cuda::cudaStats();
    
    EXPECT_EQ(&stats1, &stats2);
}

#else  // !ORTEAF_STATS_LEVEL_CUDA_VALUE

/**
 * @brief Test that statistics are disabled when stats level is not set.
 */
TEST(CudaStats, DisabledWhenStatsLevelNotSet) {
    auto& stats = cuda::statsInstance();
    
    // All update methods should be no-ops
    EXPECT_NO_THROW(stats.updateAlloc(1024));
    EXPECT_NO_THROW(stats.updateDealloc(1024));
    EXPECT_NO_THROW(stats.updateDeviceSwitch());
    EXPECT_NO_THROW(stats.updateCreateEvent());
    EXPECT_NO_THROW(stats.updateDestroyEvent());
    EXPECT_NO_THROW(stats.updateCreateStream());
    EXPECT_NO_THROW(stats.updateDestroyStream());
    EXPECT_NO_THROW(stats.updateActiveEvent());
    
    // toString should indicate disabled state
    std::string str = stats.toString();
    EXPECT_NE(str.find("Disabled"), std::string::npos);
}

#endif  // ORTEAF_STATS_LEVEL_CUDA_VALUE

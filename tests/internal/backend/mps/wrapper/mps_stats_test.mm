/**
 * @file mps_stats_test.mm
 * @brief Tests for MPS statistics tracking.
 */

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

#include "orteaf/internal/backend/mps/wrapper/mps_stats.h"
#include "orteaf/internal/backend/mps/wrapper/mps_device.h"
#include "orteaf/internal/backend/mps/wrapper/mps_command_queue.h"
#include "orteaf/internal/backend/mps/wrapper/mps_event.h"

#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace mps = orteaf::internal::backend::mps;

#if defined(ORTEAF_STATS_LEVEL_MPS_VALUE)

/**
 * @brief Test fixture for MPS stats tests.
 */
class MpsStatsTest : public ::testing::Test {
protected:
    void SetUp() override {
        device_ = mps::getDevice();
        if (device_ == nullptr) {
            GTEST_SKIP() << "No Metal devices available";
        }
    }
    
    void TearDown() override {
        if (device_ != nullptr) {
            mps::deviceRelease(device_);
        }
    }
    
    mps::MPSDevice_t device_ = nullptr;
};

#if ORTEAF_STATS_LEVEL_MPS_VALUE <= 2

/**
 * @brief Test that update_alloc increments counters.
 */
TEST_F(MpsStatsTest, UpdateAllocIncrementsCounters) {
    auto& stats = mps::statsInstance();
    uint64_t initial = stats.totalAllocations();
    
    mps::updateAlloc(1024);
    
    EXPECT_EQ(stats.totalAllocations(), initial + 1);
    EXPECT_EQ(stats.activeAllocations(), 1);
}

/**
 * @brief Test that update_dealloc decrements active allocations.
 */
TEST_F(MpsStatsTest, UpdateDeallocDecrementsActive) {
    auto& stats = mps::statsInstance();
    
    mps::updateAlloc(1024);
    EXPECT_EQ(stats.activeAllocations(), 1);
    EXPECT_EQ(stats.totalDeallocations(), 0);
    
    mps::updateDealloc(1024);
    EXPECT_EQ(stats.activeAllocations(), 0);
    EXPECT_EQ(stats.totalDeallocations(), 1);
}

/**
 * @brief Test that multiple allocations are tracked correctly.
 */
TEST_F(MpsStatsTest, MultipleAllocationsTracked) {
    auto& stats = mps::statsInstance();
    uint64_t initial_total = stats.totalAllocations();
    uint64_t initial_active = stats.activeAllocations();
    
    mps::updateAlloc(1024);
    mps::updateAlloc(2048);
    mps::updateAlloc(4096);
    
    EXPECT_EQ(stats.totalAllocations(), initial_total + 3);
    EXPECT_EQ(stats.activeAllocations(), initial_active + 3);
    
    mps::updateDealloc(1024);
    EXPECT_EQ(stats.activeAllocations(), initial_active + 2);
    
    mps::updateDealloc(2048);
    mps::updateDealloc(4096);
    EXPECT_EQ(stats.activeAllocations(), initial_active);
}

#endif  // ORTEAF_STATS_LEVEL_MPS_VALUE <= 2

#if ORTEAF_STATS_LEVEL_MPS_VALUE <= 4

/**
 * @brief Test that current_allocated_bytes tracks correctly.
 */
TEST_F(MpsStatsTest, CurrentAllocatedBytesTracked) {
    auto& stats = mps::statsInstance();
    uint64_t initial = stats.currentAllocatedBytes();
    
    mps::updateAlloc(1024);
    EXPECT_EQ(stats.currentAllocatedBytes(), initial + 1024);
    
    mps::updateAlloc(2048);
    EXPECT_EQ(stats.currentAllocatedBytes(), initial + 1024 + 2048);
    
    mps::updateDealloc(1024);
    EXPECT_EQ(stats.currentAllocatedBytes(), initial + 2048);
}

/**
 * @brief Test that peak_allocated_bytes tracks maximum.
 */
TEST_F(MpsStatsTest, PeakAllocatedBytesTracksMaximum) {
    auto& stats = mps::statsInstance();
    uint64_t initial_peak = stats.peakAllocatedBytes();
    uint64_t initial_current = stats.currentAllocatedBytes();
    
    mps::updateAlloc(1024);
    uint64_t peak1 = stats.peakAllocatedBytes();
    EXPECT_GE(peak1, initial_peak);
    
    mps::updateAlloc(2048);
    uint64_t peak2 = stats.peakAllocatedBytes();
    EXPECT_GE(peak2, peak1);
    
    mps::updateDealloc(1024);
    uint64_t peak3 = stats.peakAllocatedBytes();
    EXPECT_EQ(peak3, peak2);  // Peak should not decrease
    
    mps::updateDealloc(2048);
    EXPECT_EQ(stats.currentAllocatedBytes(), initial_current);
    EXPECT_EQ(stats.peakAllocatedBytes(), peak2);  // Peak should remain
}

/**
 * @brief Test that update_create_event increments active_events.
 */
TEST_F(MpsStatsTest, UpdateCreateEventIncrements) {
    auto& stats = mps::statsInstance();
    uint64_t initial = stats.activeEvents();
    
    mps::updateCreateEvent();
    EXPECT_EQ(stats.activeEvents(), initial + 1);
    
    mps::updateCreateEvent();
    EXPECT_EQ(stats.activeEvents(), initial + 2);
}

/**
 * @brief Test that update_destroy_event decrements active_events.
 */
TEST_F(MpsStatsTest, UpdateDestroyEventDecrements) {
    auto& stats = mps::statsInstance();
    
    mps::updateCreateEvent();
    mps::updateCreateEvent();
    EXPECT_EQ(stats.activeEvents(), 2);
    
    mps::updateDestroyEvent();
    EXPECT_EQ(stats.activeEvents(), 1);
    
    mps::updateDestroyEvent();
    EXPECT_EQ(stats.activeEvents(), 0);
}

/**
 * @brief Test that update_create_command_queue increments active_streams.
 */
TEST_F(MpsStatsTest, UpdateCreateCommandQueueIncrements) {
    auto& stats = mps::statsInstance();
    uint64_t initial = stats.activeStreams();
    
    mps::updateCreateCommandQueue();
    EXPECT_EQ(stats.activeStreams(), initial + 1);
    
    mps::updateCreateCommandQueue();
    EXPECT_EQ(stats.activeStreams(), initial + 2);
}

/**
 * @brief Test that update_destroy_command_queue decrements active_streams.
 */
TEST_F(MpsStatsTest, UpdateDestroyCommandQueueDecrements) {
    auto& stats = mps::statsInstance();
    
    mps::updateCreateCommandQueue();
    mps::updateCreateCommandQueue();
    EXPECT_EQ(stats.activeStreams(), 2);
    
    mps::updateDestroyCommandQueue();
    EXPECT_EQ(stats.activeStreams(), 1);
    
    mps::updateDestroyCommandQueue();
    EXPECT_EQ(stats.activeStreams(), 0);
}

/**
 * @brief Test that statistics are updated when creating/destroying command queues.
 */
TEST_F(MpsStatsTest, CommandQueueCreationUpdatesStats) {
    auto& stats = mps::statsInstance();
    uint64_t initial_streams = stats.activeStreams();
    
    mps::MPSCommandQueue_t queue = mps::createCommandQueue(device_);
    if (queue != nullptr) {
        EXPECT_EQ(stats.activeStreams(), initial_streams + 1);
        mps::destroyCommandQueue(queue);
        EXPECT_EQ(stats.activeStreams(), initial_streams);
    }
}

/**
 * @brief Test that statistics are updated when creating/destroying events.
 */
TEST_F(MpsStatsTest, EventCreationUpdatesStats) {
    auto& stats = mps::statsInstance();
    uint64_t initial_events = stats.activeEvents();
    
    mps::MPSEvent_t event = mps::createEvent(device_);
    if (event != nullptr) {
        EXPECT_EQ(stats.activeEvents(), initial_events + 1);
        mps::destroyEvent(event);
        EXPECT_EQ(stats.activeEvents(), initial_events);
    }
}

#endif  // ORTEAF_STATS_LEVEL_MPS_VALUE <= 4

/**
 * @brief Test that to_string produces valid output.
 */
TEST_F(MpsStatsTest, ToStringProducesValidOutput) {
    auto& stats = mps::statsInstance();
    std::string str = stats.toString();
    
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("MPS Stats"), std::string::npos);
}

/**
 * @brief Test that to_string is not empty even with no operations.
 */
TEST_F(MpsStatsTest, ToStringNotEmptyWhenEmpty) {
    auto& stats = mps::statsInstance();
    std::string str = stats.toString();
    
    EXPECT_FALSE(str.empty());
}

/**
 * @brief Test that stream output operator works.
 */
TEST_F(MpsStatsTest, StreamOutputOperatorWorks) {
    auto& stats = mps::statsInstance();
    std::ostringstream oss;
    
    oss << stats;
    
    EXPECT_FALSE(oss.str().empty());
}

/**
 * @brief Test that stats_instance returns singleton.
 */
TEST_F(MpsStatsTest, StatsInstanceIsSingleton) {
    mps::MpsStats& stats1 = mps::statsInstance();
    mps::MpsStats& stats2 = mps::statsInstance();
    
    EXPECT_EQ(&stats1, &stats2);
}

/**
 * @brief Test that mps_stats returns same instance.
 */
TEST_F(MpsStatsTest, MpsStatsReturnsSameInstance) {
    mps::MpsStats& stats1 = mps::statsInstance();
    mps::MpsStats& stats2 = mps::mpsStats();
    
    EXPECT_EQ(&stats1, &stats2);
}

/**
 * @brief Test that statistics are thread-safe.
 */
#if ORTEAF_STATS_LEVEL_MPS_VALUE <= 2
TEST_F(MpsStatsTest, StatisticsAreThreadSafe) {
    auto& stats = mps::statsInstance();
    constexpr int num_threads = 4;
    constexpr int ops_per_thread = 100;
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&stats]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                mps::updateAlloc(1024);
                mps::updateDealloc(1024);
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
#endif  // ORTEAF_STATS_LEVEL_MPS_VALUE <= 2

#else  // !ORTEAF_STATS_LEVEL_MPS_VALUE

/**
 * @brief Test that statistics are disabled when stats level is not set.
 */
TEST(MpsStats, DisabledWhenStatsLevelNotSet) {
    auto& stats = mps::statsInstance();
    
    // All update methods should be no-ops
    EXPECT_NO_THROW(stats.updateAlloc(1024));
    EXPECT_NO_THROW(stats.updateDealloc(1024));
    EXPECT_NO_THROW(stats.updateCreateEvent());
    EXPECT_NO_THROW(stats.updateDestroyEvent());
    EXPECT_NO_THROW(stats.updateCreateStream());
    EXPECT_NO_THROW(stats.updateDestroyStream());
    EXPECT_NO_THROW(stats.updateActiveEvent());
    EXPECT_NO_THROW(stats.updateCreateCommandQueue());
    EXPECT_NO_THROW(stats.updateDestroyCommandQueue());
    
    // to_string should indicate disabled state
    std::string str = stats.toString();
    EXPECT_NE(str.find("Disabled"), std::string::npos);
}

#endif  // ORTEAF_STATS_LEVEL_MPS_VALUE

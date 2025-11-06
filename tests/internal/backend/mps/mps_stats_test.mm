/**
 * @file mps_stats_test.mm
 * @brief Tests for MPS statistics tracking.
 */

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

#include "orteaf/internal/backend/mps/mps_stats.h"
#include "orteaf/internal/backend/mps/mps_device.h"
#include "orteaf/internal/backend/mps/mps_command_queue.h"
#include "orteaf/internal/backend/mps/mps_event.h"

#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace mps = orteaf::internal::backend::mps;

#if defined(ORTEAF_ENABLE_MPS) && defined(ORTEAF_STATS_LEVEL_MPS_VALUE)

/**
 * @brief Test fixture for MPS stats tests.
 */
class MpsStatsTest : public ::testing::Test {
protected:
    void SetUp() override {
        device_ = mps::get_device();
        if (device_ == nullptr) {
            GTEST_SKIP() << "No Metal devices available";
        }
    }
    
    void TearDown() override {
        if (device_ != nullptr) {
            mps::device_release(device_);
        }
    }
    
    mps::MPSDevice_t device_ = nullptr;
};

#if ORTEAF_STATS_LEVEL_MPS_VALUE <= 2

/**
 * @brief Test that update_alloc increments counters.
 */
TEST_F(MpsStatsTest, UpdateAllocIncrementsCounters) {
    auto& stats = mps::stats_instance();
    uint64_t initial = stats.total_allocations();
    
    mps::update_alloc(1024);
    
    EXPECT_EQ(stats.total_allocations(), initial + 1);
    EXPECT_EQ(stats.active_allocations(), 1);
}

/**
 * @brief Test that update_dealloc decrements active allocations.
 */
TEST_F(MpsStatsTest, UpdateDeallocDecrementsActive) {
    auto& stats = mps::stats_instance();
    
    mps::update_alloc(1024);
    EXPECT_EQ(stats.active_allocations(), 1);
    EXPECT_EQ(stats.total_deallocations(), 0);
    
    mps::update_dealloc(1024);
    EXPECT_EQ(stats.active_allocations(), 0);
    EXPECT_EQ(stats.total_deallocations(), 1);
}

/**
 * @brief Test that multiple allocations are tracked correctly.
 */
TEST_F(MpsStatsTest, MultipleAllocationsTracked) {
    auto& stats = mps::stats_instance();
    uint64_t initial_total = stats.total_allocations();
    uint64_t initial_active = stats.active_allocations();
    
    mps::update_alloc(1024);
    mps::update_alloc(2048);
    mps::update_alloc(4096);
    
    EXPECT_EQ(stats.total_allocations(), initial_total + 3);
    EXPECT_EQ(stats.active_allocations(), initial_active + 3);
    
    mps::update_dealloc(1024);
    EXPECT_EQ(stats.active_allocations(), initial_active + 2);
    
    mps::update_dealloc(2048);
    mps::update_dealloc(4096);
    EXPECT_EQ(stats.active_allocations(), initial_active);
}

#endif  // ORTEAF_STATS_LEVEL_MPS_VALUE <= 2

#if ORTEAF_STATS_LEVEL_MPS_VALUE <= 4

/**
 * @brief Test that current_allocated_bytes tracks correctly.
 */
TEST_F(MpsStatsTest, CurrentAllocatedBytesTracked) {
    auto& stats = mps::stats_instance();
    uint64_t initial = stats.current_allocated_bytes();
    
    mps::update_alloc(1024);
    EXPECT_EQ(stats.current_allocated_bytes(), initial + 1024);
    
    mps::update_alloc(2048);
    EXPECT_EQ(stats.current_allocated_bytes(), initial + 1024 + 2048);
    
    mps::update_dealloc(1024);
    EXPECT_EQ(stats.current_allocated_bytes(), initial + 2048);
}

/**
 * @brief Test that peak_allocated_bytes tracks maximum.
 */
TEST_F(MpsStatsTest, PeakAllocatedBytesTracksMaximum) {
    auto& stats = mps::stats_instance();
    uint64_t initial_peak = stats.peak_allocated_bytes();
    uint64_t initial_current = stats.current_allocated_bytes();
    
    mps::update_alloc(1024);
    uint64_t peak1 = stats.peak_allocated_bytes();
    EXPECT_GE(peak1, initial_peak);
    
    mps::update_alloc(2048);
    uint64_t peak2 = stats.peak_allocated_bytes();
    EXPECT_GE(peak2, peak1);
    
    mps::update_dealloc(1024);
    uint64_t peak3 = stats.peak_allocated_bytes();
    EXPECT_EQ(peak3, peak2);  // Peak should not decrease
    
    mps::update_dealloc(2048);
    EXPECT_EQ(stats.current_allocated_bytes(), initial_current);
    EXPECT_EQ(stats.peak_allocated_bytes(), peak2);  // Peak should remain
}

/**
 * @brief Test that update_create_event increments active_events.
 */
TEST_F(MpsStatsTest, UpdateCreateEventIncrements) {
    auto& stats = mps::stats_instance();
    uint64_t initial = stats.active_events();
    
    mps::update_create_event();
    EXPECT_EQ(stats.active_events(), initial + 1);
    
    mps::update_create_event();
    EXPECT_EQ(stats.active_events(), initial + 2);
}

/**
 * @brief Test that update_destroy_event decrements active_events.
 */
TEST_F(MpsStatsTest, UpdateDestroyEventDecrements) {
    auto& stats = mps::stats_instance();
    
    mps::update_create_event();
    mps::update_create_event();
    EXPECT_EQ(stats.active_events(), 2);
    
    mps::update_destroy_event();
    EXPECT_EQ(stats.active_events(), 1);
    
    mps::update_destroy_event();
    EXPECT_EQ(stats.active_events(), 0);
}

/**
 * @brief Test that update_create_command_queue increments active_streams.
 */
TEST_F(MpsStatsTest, UpdateCreateCommandQueueIncrements) {
    auto& stats = mps::stats_instance();
    uint64_t initial = stats.active_streams();
    
    mps::update_create_command_queue();
    EXPECT_EQ(stats.active_streams(), initial + 1);
    
    mps::update_create_command_queue();
    EXPECT_EQ(stats.active_streams(), initial + 2);
}

/**
 * @brief Test that update_destroy_command_queue decrements active_streams.
 */
TEST_F(MpsStatsTest, UpdateDestroyCommandQueueDecrements) {
    auto& stats = mps::stats_instance();
    
    mps::update_create_command_queue();
    mps::update_create_command_queue();
    EXPECT_EQ(stats.active_streams(), 2);
    
    mps::update_destroy_command_queue();
    EXPECT_EQ(stats.active_streams(), 1);
    
    mps::update_destroy_command_queue();
    EXPECT_EQ(stats.active_streams(), 0);
}

/**
 * @brief Test that statistics are updated when creating/destroying command queues.
 */
TEST_F(MpsStatsTest, CommandQueueCreationUpdatesStats) {
    auto& stats = mps::stats_instance();
    uint64_t initial_streams = stats.active_streams();
    
    mps::MPSCommandQueue_t queue = mps::create_command_queue(device_);
    if (queue != nullptr) {
        EXPECT_EQ(stats.active_streams(), initial_streams + 1);
        mps::destroy_command_queue(queue);
        EXPECT_EQ(stats.active_streams(), initial_streams);
    }
}

/**
 * @brief Test that statistics are updated when creating/destroying events.
 */
TEST_F(MpsStatsTest, EventCreationUpdatesStats) {
    auto& stats = mps::stats_instance();
    uint64_t initial_events = stats.active_events();
    
    mps::MPSEvent_t event = mps::create_event(device_);
    if (event != nullptr) {
        EXPECT_EQ(stats.active_events(), initial_events + 1);
        mps::destroy_event(event);
        EXPECT_EQ(stats.active_events(), initial_events);
    }
}

#endif  // ORTEAF_STATS_LEVEL_MPS_VALUE <= 4

/**
 * @brief Test that to_string produces valid output.
 */
TEST_F(MpsStatsTest, ToStringProducesValidOutput) {
    auto& stats = mps::stats_instance();
    std::string str = stats.to_string();
    
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("MPS Stats"), std::string::npos);
}

/**
 * @brief Test that to_string is not empty even with no operations.
 */
TEST_F(MpsStatsTest, ToStringNotEmptyWhenEmpty) {
    auto& stats = mps::stats_instance();
    std::string str = stats.to_string();
    
    EXPECT_FALSE(str.empty());
}

/**
 * @brief Test that stream output operator works.
 */
TEST_F(MpsStatsTest, StreamOutputOperatorWorks) {
    auto& stats = mps::stats_instance();
    std::ostringstream oss;
    
    oss << stats;
    
    EXPECT_FALSE(oss.str().empty());
}

/**
 * @brief Test that stats_instance returns singleton.
 */
TEST_F(MpsStatsTest, StatsInstanceIsSingleton) {
    mps::MpsStats& stats1 = mps::stats_instance();
    mps::MpsStats& stats2 = mps::stats_instance();
    
    EXPECT_EQ(&stats1, &stats2);
}

/**
 * @brief Test that mps_stats returns same instance.
 */
TEST_F(MpsStatsTest, MpsStatsReturnsSameInstance) {
    mps::MpsStats& stats1 = mps::stats_instance();
    mps::MpsStats& stats2 = mps::mps_stats();
    
    EXPECT_EQ(&stats1, &stats2);
}

/**
 * @brief Test that statistics are thread-safe.
 */
#if ORTEAF_STATS_LEVEL_MPS_VALUE <= 2
TEST_F(MpsStatsTest, StatisticsAreThreadSafe) {
    auto& stats = mps::stats_instance();
    constexpr int num_threads = 4;
    constexpr int ops_per_thread = 100;
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&stats]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                mps::update_alloc(1024);
                mps::update_dealloc(1024);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // All allocations should be deallocated
    EXPECT_EQ(stats.active_allocations(), 0);
    EXPECT_EQ(stats.total_allocations(), num_threads * ops_per_thread);
    EXPECT_EQ(stats.total_deallocations(), num_threads * ops_per_thread);
}
#endif  // ORTEAF_STATS_LEVEL_MPS_VALUE <= 2

#else  // !ORTEAF_ENABLE_MPS || !ORTEAF_STATS_LEVEL_MPS_VALUE

/**
 * @brief Test that statistics are disabled when stats level is not set.
 */
TEST(MpsStats, DisabledWhenStatsLevelNotSet) {
    auto& stats = mps::stats_instance();
    
    // All update methods should be no-ops
    EXPECT_NO_THROW(stats.update_alloc(1024));
    EXPECT_NO_THROW(stats.update_dealloc(1024));
    EXPECT_NO_THROW(stats.update_create_event());
    EXPECT_NO_THROW(stats.update_destroy_event());
    EXPECT_NO_THROW(stats.update_create_stream());
    EXPECT_NO_THROW(stats.update_destroy_stream());
    EXPECT_NO_THROW(stats.update_active_event());
    EXPECT_NO_THROW(stats.update_create_command_queue());
    EXPECT_NO_THROW(stats.update_destroy_command_queue());
    
    // to_string should indicate disabled state
    std::string str = stats.to_string();
    EXPECT_NE(str.find("Disabled"), std::string::npos);
}

#endif  // ORTEAF_ENABLE_MPS && ORTEAF_STATS_LEVEL_MPS_VALUE

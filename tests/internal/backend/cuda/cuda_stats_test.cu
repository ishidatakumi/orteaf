/**
 * @file cuda_stats_test.cpp
 * @brief Tests for CUDA statistics tracking.
 */

#include "orteaf/internal/backend/cuda/cuda_stats.h"
#include "orteaf/internal/backend/cuda/cuda_init.h"
#include "orteaf/internal/backend/cuda/cuda_device.h"
#include "orteaf/internal/backend/cuda/cuda_context.h"
#include "orteaf/internal/backend/cuda/cuda_stream.h"
#include "orteaf/internal/backend/cuda/cuda_event.h"

#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace cuda = orteaf::internal::backend::cuda;

#if defined(ORTEAF_ENABLE_CUDA) && defined(ORTEAF_STATS_LEVEL_CUDA_VALUE)

/**
 * @brief Test fixture that initializes CUDA and resets stats.
 */
class CudaStatsTest : public ::testing::Test {
protected:
    void SetUp() override {
        cuda::cuda_init();
        int count = cuda::get_device_count();
        if (count == 0) {
            GTEST_SKIP() << "No CUDA devices available";
        }
        device_ = cuda::get_device(0);
        context_ = cuda::get_primary_context(device_);
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

#if ORTEAF_STATS_LEVEL_CUDA_VALUE <= 2

/**
 * @brief Test that update_alloc increments counters.
 */
TEST_F(CudaStatsTest, UpdateAllocIncrementsCounters) {
    auto& stats = cuda::stats_instance();
    uint64_t initial = stats.total_allocations();
    
    cuda::update_alloc(1024);
    
    EXPECT_EQ(stats.total_allocations(), initial + 1);
    EXPECT_EQ(stats.active_allocations(), 1);
}

/**
 * @brief Test that update_dealloc decrements active allocations.
 */
TEST_F(CudaStatsTest, UpdateDeallocDecrementsActive) {
    auto& stats = cuda::stats_instance();
    
    cuda::update_alloc(1024);
    EXPECT_EQ(stats.active_allocations(), 1);
    EXPECT_EQ(stats.total_deallocations(), 0);
    
    cuda::update_dealloc(1024);
    EXPECT_EQ(stats.active_allocations(), 0);
    EXPECT_EQ(stats.total_deallocations(), 1);
}

/**
 * @brief Test that multiple allocations are tracked correctly.
 */
TEST_F(CudaStatsTest, MultipleAllocationsTracked) {
    auto& stats = cuda::stats_instance();
    uint64_t initial_total = stats.total_allocations();
    uint64_t initial_active = stats.active_allocations();
    
    cuda::update_alloc(1024);
    cuda::update_alloc(2048);
    cuda::update_alloc(4096);
    
    EXPECT_EQ(stats.total_allocations(), initial_total + 3);
    EXPECT_EQ(stats.active_allocations(), initial_active + 3);
    
    cuda::update_dealloc(1024);
    EXPECT_EQ(stats.active_allocations(), initial_active + 2);
    
    cuda::update_dealloc(2048);
    cuda::update_dealloc(4096);
    EXPECT_EQ(stats.active_allocations(), initial_active);
}

/**
 * @brief Test that update_device_switch increments counter.
 */
TEST_F(CudaStatsTest, UpdateDeviceSwitchIncrements) {
    auto& stats = cuda::stats_instance();
    uint64_t initial = stats.device_switches();
    
    cuda::update_device_switch();
    EXPECT_EQ(stats.device_switches(), initial + 1);
    
    cuda::update_device_switch();
    EXPECT_EQ(stats.device_switches(), initial + 2);
}

/**
 * @brief Test that statistics are thread-safe.
 */
TEST_F(CudaStatsTest, StatisticsAreThreadSafe) {
    auto& stats = cuda::stats_instance();
    constexpr int num_threads = 4;
    constexpr int ops_per_thread = 100;
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&stats]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                cuda::update_alloc(1024);
                cuda::update_dealloc(1024);
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

#endif  // ORTEAF_STATS_LEVEL_CUDA_VALUE <= 2

#if ORTEAF_STATS_LEVEL_CUDA_VALUE <= 4

/**
 * @brief Test that current_allocated_bytes tracks correctly.
 */
TEST_F(CudaStatsTest, CurrentAllocatedBytesTracked) {
    auto& stats = cuda::stats_instance();
    uint64_t initial = stats.current_allocated_bytes();
    
    cuda::update_alloc(1024);
    EXPECT_EQ(stats.current_allocated_bytes(), initial + 1024);
    
    cuda::update_alloc(2048);
    EXPECT_EQ(stats.current_allocated_bytes(), initial + 1024 + 2048);
    
    cuda::update_dealloc(1024);
    EXPECT_EQ(stats.current_allocated_bytes(), initial + 2048);
}

/**
 * @brief Test that peak_allocated_bytes tracks maximum.
 */
TEST_F(CudaStatsTest, PeakAllocatedBytesTracksMaximum) {
    auto& stats = cuda::stats_instance();
    uint64_t initial_peak = stats.peak_allocated_bytes();
    uint64_t initial_current = stats.current_allocated_bytes();
    
    cuda::update_alloc(1024);
    uint64_t peak1 = stats.peak_allocated_bytes();
    EXPECT_GE(peak1, initial_peak);
    
    cuda::update_alloc(2048);
    uint64_t peak2 = stats.peak_allocated_bytes();
    EXPECT_GE(peak2, peak1);
    
    cuda::update_dealloc(1024);
    uint64_t peak3 = stats.peak_allocated_bytes();
    EXPECT_EQ(peak3, peak2);  // Peak should not decrease
    
    cuda::update_dealloc(2048);
    EXPECT_EQ(stats.current_allocated_bytes(), initial_current);
    EXPECT_EQ(stats.peak_allocated_bytes(), peak2);  // Peak should remain
}

/**
 * @brief Test that update_create_event increments active_events.
 */
TEST_F(CudaStatsTest, UpdateCreateEventIncrements) {
    auto& stats = cuda::stats_instance();
    uint64_t initial = stats.active_events();
    
    cuda::update_create_event();
    EXPECT_EQ(stats.active_events(), initial + 1);
    
    cuda::update_create_event();
    EXPECT_EQ(stats.active_events(), initial + 2);
}

/**
 * @brief Test that update_destroy_event decrements active_events.
 */
TEST_F(CudaStatsTest, UpdateDestroyEventDecrements) {
    auto& stats = cuda::stats_instance();
    
    cuda::update_create_event();
    cuda::update_create_event();
    EXPECT_EQ(stats.active_events(), 2);
    
    cuda::update_destroy_event();
    EXPECT_EQ(stats.active_events(), 1);
    
    cuda::update_destroy_event();
    EXPECT_EQ(stats.active_events(), 0);
}

/**
 * @brief Test that update_create_stream increments active_streams.
 */
TEST_F(CudaStatsTest, UpdateCreateStreamIncrements) {
    auto& stats = cuda::stats_instance();
    uint64_t initial = stats.active_streams();
    
    cuda::update_create_stream();
    EXPECT_EQ(stats.active_streams(), initial + 1);
    
    cuda::update_create_stream();
    EXPECT_EQ(stats.active_streams(), initial + 2);
}

/**
 * @brief Test that update_destroy_stream decrements active_streams.
 */
TEST_F(CudaStatsTest, UpdateDestroyStreamDecrements) {
    auto& stats = cuda::stats_instance();
    
    cuda::update_create_stream();
    cuda::update_create_stream();
    EXPECT_EQ(stats.active_streams(), 2);
    
    cuda::update_destroy_stream();
    EXPECT_EQ(stats.active_streams(), 1);
    
    cuda::update_destroy_stream();
    EXPECT_EQ(stats.active_streams(), 0);
}

/**
 * @brief Test that update_active_event increments active_events.
 */
TEST_F(CudaStatsTest, UpdateActiveEventIncrements) {
    auto& stats = cuda::stats_instance();
    uint64_t initial = stats.active_events();
    
    cuda::update_active_event();
    EXPECT_EQ(stats.active_events(), initial + 1);
    
    cuda::update_active_event();
    EXPECT_EQ(stats.active_events(), initial + 2);
}

/**
 * @brief Test that statistics are updated when creating/destroying streams.
 */
TEST_F(CudaStatsTest, StreamCreationUpdatesStats) {
    auto& stats = cuda::stats_instance();
    uint64_t initial_streams = stats.active_streams();
    
    cuda::CUstream_t stream1 = cuda::get_stream();
    EXPECT_EQ(stats.active_streams(), initial_streams + 1);
    
    cuda::CUstream_t stream2 = cuda::get_stream();
    EXPECT_EQ(stats.active_streams(), initial_streams + 2);
    
    cuda::release_stream(stream1);
    EXPECT_EQ(stats.active_streams(), initial_streams + 1);
    
    cuda::release_stream(stream2);
    EXPECT_EQ(stats.active_streams(), initial_streams);
}

/**
 * @brief Test that statistics are updated when creating/destroying events.
 */
TEST_F(CudaStatsTest, EventCreationUpdatesStats) {
    auto& stats = cuda::stats_instance();
    uint64_t initial_events = stats.active_events();
    
    cuda::CUevent_t event1 = cuda::create_event();
    EXPECT_EQ(stats.active_events(), initial_events + 1);
    
    cuda::CUevent_t event2 = cuda::create_event();
    EXPECT_EQ(stats.active_events(), initial_events + 2);
    
    cuda::destroy_event(event1);
    EXPECT_EQ(stats.active_events(), initial_events + 1);
    
    cuda::destroy_event(event2);
    EXPECT_EQ(stats.active_events(), initial_events);
}

#endif  // ORTEAF_STATS_LEVEL_CUDA_VALUE <= 4

/**
 * @brief Test that to_string produces valid output.
 */
TEST_F(CudaStatsTest, ToStringProducesValidOutput) {
    auto& stats = cuda::stats_instance();
    std::string str = stats.to_string();
    
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("CUDA Stats"), std::string::npos);
}

/**
 * @brief Test that to_string is not empty even with no operations.
 */
TEST_F(CudaStatsTest, ToStringNotEmptyWhenEmpty) {
    auto& stats = cuda::stats_instance();
    std::string str = stats.to_string();
    
    EXPECT_FALSE(str.empty());
}

/**
 * @brief Test that stream output operator works.
 */
TEST_F(CudaStatsTest, StreamOutputOperatorWorks) {
    auto& stats = cuda::stats_instance();
    std::ostringstream oss;
    
    oss << stats;
    
    EXPECT_FALSE(oss.str().empty());
}

/**
 * @brief Test that stats_instance returns singleton.
 */
TEST_F(CudaStatsTest, StatsInstanceIsSingleton) {
    cuda::CudaStats& stats1 = cuda::stats_instance();
    cuda::CudaStats& stats2 = cuda::stats_instance();
    
    EXPECT_EQ(&stats1, &stats2);
}

/**
 * @brief Test that cuda_stats returns same instance.
 */
TEST_F(CudaStatsTest, CudaStatsReturnsSameInstance) {
    cuda::CudaStats& stats1 = cuda::stats_instance();
    cuda::CudaStats& stats2 = cuda::cuda_stats();
    
    EXPECT_EQ(&stats1, &stats2);
}

#else  // !ORTEAF_ENABLE_CUDA || !ORTEAF_STATS_LEVEL_CUDA_VALUE

/**
 * @brief Test that statistics are disabled when stats level is not set.
 */
TEST(CudaStats, DisabledWhenStatsLevelNotSet) {
    auto& stats = cuda::stats_instance();
    
    // All update methods should be no-ops
    EXPECT_NO_THROW(stats.update_alloc(1024));
    EXPECT_NO_THROW(stats.update_dealloc(1024));
    EXPECT_NO_THROW(stats.update_device_switch());
    EXPECT_NO_THROW(stats.update_create_event());
    EXPECT_NO_THROW(stats.update_destroy_event());
    EXPECT_NO_THROW(stats.update_create_stream());
    EXPECT_NO_THROW(stats.update_destroy_stream());
    EXPECT_NO_THROW(stats.update_active_event());
    
    // to_string should indicate disabled state
    std::string str = stats.to_string();
    EXPECT_NE(str.find("Disabled"), std::string::npos);
}

#endif  // ORTEAF_ENABLE_CUDA && ORTEAF_STATS_LEVEL_CUDA_VALUE

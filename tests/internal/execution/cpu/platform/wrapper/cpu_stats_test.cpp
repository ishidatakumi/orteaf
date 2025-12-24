/**
 * @file cpu_stats_test.cpp
 * @brief Tests for CPU statistics tracking.
 */

#include "orteaf/internal/execution/cpu/platform/wrapper/cpu_stats.h"
#include "orteaf/internal/execution/cpu/platform/wrapper/cpu_alloc.h"

#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace cpu = orteaf::internal::execution::cpu::platform::wrapper;

#if defined(ORTEAF_ENABLE_CPU) && defined(ORTEAF_STATS_LEVEL_CPU_VALUE)

/**
 * @brief Test fixture for CPU stats tests.
 */
class CpuStatsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // CPU stats don't require explicit initialization
    }
};

#if ORTEAF_STATS_LEVEL_CPU_VALUE <= 2

/**
 * @brief Test that update_alloc increments counters.
 */
TEST_F(CpuStatsTest, UpdateAllocIncrementsCounters) {
    auto& stats = cpu::statsInstance();
    uint64_t initial = stats.totalAllocations();
    
    cpu::updateAlloc(1024);
    
    EXPECT_EQ(stats.totalAllocations(), initial + 1);
    EXPECT_EQ(stats.activeAllocations(), 1);
}

/**
 * @brief Test that update_dealloc decrements active allocations.
 */
TEST_F(CpuStatsTest, UpdateDeallocDecrementsActive) {
    auto& stats = cpu::statsInstance();
    
    cpu::updateAlloc(1024);
    EXPECT_EQ(stats.activeAllocations(), 1);
    EXPECT_EQ(stats.totalDeallocations(), 0);
    
    cpu::updateDealloc(1024);
    EXPECT_EQ(stats.activeAllocations(), 0);
    EXPECT_EQ(stats.totalDeallocations(), 1);
}

/**
 * @brief Test that multiple allocations are tracked correctly.
 */
TEST_F(CpuStatsTest, MultipleAllocationsTracked) {
    auto& stats = cpu::statsInstance();
    uint64_t initial_total = stats.totalAllocations();
    uint64_t initial_active = stats.activeAllocations();
    
    cpu::updateAlloc(1024);
    cpu::updateAlloc(2048);
    cpu::updateAlloc(4096);
    
    EXPECT_EQ(stats.totalAllocations(), initial_total + 3);
    EXPECT_EQ(stats.activeAllocations(), initial_active + 3);
    
    cpu::updateDealloc(1024);
    EXPECT_EQ(stats.activeAllocations(), initial_active + 2);
    
    cpu::updateDealloc(2048);
    cpu::updateDealloc(4096);
    EXPECT_EQ(stats.activeAllocations(), initial_active);
}

#endif  // ORTEAF_STATS_LEVEL_CPU_VALUE <= 2

#if ORTEAF_STATS_LEVEL_CPU_VALUE <= 4

/**
 * @brief Test that current_allocated_bytes tracks correctly.
 */
TEST_F(CpuStatsTest, CurrentAllocatedBytesTracked) {
    auto& stats = cpu::statsInstance();
    uint64_t initial = stats.currentAllocatedBytes();
    
    cpu::updateAlloc(1024);
    EXPECT_EQ(stats.currentAllocatedBytes(), initial + 1024);
    
    cpu::updateAlloc(2048);
    EXPECT_EQ(stats.currentAllocatedBytes(), initial + 1024 + 2048);
    
    cpu::updateDealloc(1024);
    EXPECT_EQ(stats.currentAllocatedBytes(), initial + 2048);
}

/**
 * @brief Test that peak_allocated_bytes tracks maximum.
 */
TEST_F(CpuStatsTest, PeakAllocatedBytesTracksMaximum) {
    auto& stats = cpu::statsInstance();
    uint64_t initial_peak = stats.peakAllocatedBytes();
    uint64_t initial_current = stats.currentAllocatedBytes();
    
    cpu::updateAlloc(1024);
    uint64_t peak1 = stats.peakAllocatedBytes();
    EXPECT_GE(peak1, initial_peak);
    
    cpu::updateAlloc(2048);
    uint64_t peak2 = stats.peakAllocatedBytes();
    EXPECT_GE(peak2, peak1);
    
    cpu::updateDealloc(1024);
    uint64_t peak3 = stats.peakAllocatedBytes();
    EXPECT_EQ(peak3, peak2);  // Peak should not decrease
    
    cpu::updateDealloc(2048);
    EXPECT_EQ(stats.currentAllocatedBytes(), initial_current);
    EXPECT_EQ(stats.peakAllocatedBytes(), peak2);  // Peak should remain
}

#endif  // ORTEAF_STATS_LEVEL_CPU_VALUE <= 4

#if ORTEAF_STATS_LEVEL_CPU_VALUE <= 2

/**
 * @brief Test that statistics are updated when using alloc/dealloc.
 */
TEST_F(CpuStatsTest, AllocDeallocUpdatesStats) {
    auto& stats = cpu::statsInstance();
    uint64_t initial_total = stats.totalAllocations();
    uint64_t initial_active = stats.activeAllocations();
    
    void* ptr1 = cpu::alloc(1024);
    EXPECT_EQ(stats.totalAllocations(), initial_total + 1);
    EXPECT_EQ(stats.activeAllocations(), initial_active + 1);
    
    void* ptr2 = cpu::alloc(2048);
    EXPECT_EQ(stats.totalAllocations(), initial_total + 2);
    EXPECT_EQ(stats.activeAllocations(), initial_active + 2);
    
    cpu::dealloc(ptr1, 1024);
    EXPECT_EQ(stats.activeAllocations(), initial_active + 1);
    
    cpu::dealloc(ptr2, 2048);
    EXPECT_EQ(stats.activeAllocations(), initial_active);
}

/**
 * @brief Test that statistics are thread-safe.
 */
TEST_F(CpuStatsTest, StatisticsAreThreadSafe) {
    auto& stats = cpu::statsInstance();
    constexpr int num_threads = 4;
    constexpr int ops_per_thread = 100;
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&stats]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                cpu::updateAlloc(1024);
                cpu::updateDealloc(1024);
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

/**
 * @brief Test that statistics work with actual allocations.
 */
TEST_F(CpuStatsTest, StatisticsWithActualAllocations) {
    auto& stats = cpu::statsInstance();
    uint64_t initial_total = stats.totalAllocations();
    uint64_t initial_active = stats.activeAllocations();
    
    std::vector<void*> ptrs;
    constexpr size_t size = 256;
    constexpr int num_allocs = 10;
    
    for (int i = 0; i < num_allocs; ++i) {
        ptrs.push_back(cpu::alloc(size));
    }
    
    EXPECT_EQ(stats.totalAllocations(), initial_total + num_allocs);
    EXPECT_EQ(stats.activeAllocations(), initial_active + num_allocs);
    
    for (auto ptr : ptrs) {
        cpu::dealloc(ptr, size);
    }
    
    EXPECT_EQ(stats.activeAllocations(), initial_active);
}

#endif  // ORTEAF_STATS_LEVEL_CPU_VALUE <= 2

/**
 * @brief Test that to_string produces valid output.
 */
TEST_F(CpuStatsTest, ToStringProducesValidOutput) {
    auto& stats = cpu::statsInstance();
    std::string str = stats.toString();
    
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("CPU Stats"), std::string::npos);
}

/**
 * @brief Test that to_string is not empty even with no operations.
 */
TEST_F(CpuStatsTest, ToStringNotEmptyWhenEmpty) {
    auto& stats = cpu::statsInstance();
    std::string str = stats.toString();
    
    EXPECT_FALSE(str.empty());
}

/**
 * @brief Test that stream output operator works.
 */
TEST_F(CpuStatsTest, StreamOutputOperatorWorks) {
    auto& stats = cpu::statsInstance();
    std::ostringstream oss;
    
    oss << stats;
    
    EXPECT_FALSE(oss.str().empty());
}

/**
 * @brief Test that stats_instance returns singleton.
 */
TEST_F(CpuStatsTest, StatsInstanceIsSingleton) {
    cpu::CpuStats& stats1 = cpu::statsInstance();
    cpu::CpuStats& stats2 = cpu::statsInstance();
    
    EXPECT_EQ(&stats1, &stats2);
}

/**
 * @brief Test that cpu_stats returns same instance.
 */
TEST_F(CpuStatsTest, CpuStatsReturnsSameInstance) {
    cpu::CpuStats& stats1 = cpu::statsInstance();
    cpu::CpuStats& stats2 = cpu::cpuStats();
    
    EXPECT_EQ(&stats1, &stats2);
}

#else  // !ORTEAF_ENABLE_CPU || !ORTEAF_STATS_LEVEL_CPU_VALUE

/**
 * @brief Test that statistics are disabled when stats level is not set.
 */
TEST(CpuStats, DisabledWhenStatsLevelNotSet) {
    auto& stats = cpu::statsInstance();
    
    // All update methods should be no-ops
    EXPECT_NO_THROW(stats.updateAlloc(1024));
    EXPECT_NO_THROW(stats.updateDealloc(1024));
    
    // toString should indicate disabled state
    std::string str = stats.toString();
    EXPECT_NE(str.find("Disabled"), std::string::npos);
}

#endif  // ORTEAF_ENABLE_CPU && ORTEAF_STATS_LEVEL_CPU_VALUE

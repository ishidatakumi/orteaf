#pragma once

#include <atomic>
#include <string>
#include <sstream>
#include <iostream>

namespace orteaf::internal::backend::cpu {

/**
 * @brief Statistics tracking for CPU memory allocations.
 *
 * This class tracks various statistics about CPU memory allocations and deallocations.
 * The available statistics depend on the `ORTEAF_STATS_LEVEL_CPU_VALUE` compile-time setting:
 * - STATS_BASIC (2): Tracks total allocations, total deallocations, and active allocations count.
 * - STATS_EXTENDED (4): Additionally tracks current allocated bytes and peak allocated bytes.
 * - Disabled: Provides no-op methods when statistics are disabled.
 *
 * All statistics are thread-safe and use atomic operations with relaxed memory ordering.
 * All public methods are always declared, ensuring a consistent interface regardless of
 * the statistics level configuration.
 */
class CpuStats {
public:
    // Getter methods - always defined, return values based on stats level
    /**
     * @brief Get the total number of allocations performed.
     *
     * @return Total number of memory allocations since initialization (0 if stats disabled).
     */
    uint64_t totalAllocations() const noexcept {
        #ifdef ORTEAF_STATS_LEVEL_CPU_VALUE
            #if ORTEAF_STATS_LEVEL_CPU_VALUE <= 2
                return total_allocations_.load(std::memory_order_relaxed);
            #endif
        #endif
        return 0;
    }
    
    /**
     * @brief Get the total number of deallocations performed.
     *
     * @return Total number of memory deallocations since initialization (0 if stats disabled).
     */
    uint64_t totalDeallocations() const noexcept {
        #ifdef ORTEAF_STATS_LEVEL_CPU_VALUE
            #if ORTEAF_STATS_LEVEL_CPU_VALUE <= 2
                return total_deallocations_.load(std::memory_order_relaxed);
            #endif
        #endif
        return 0;
    }
    
    /**
     * @brief Get the number of currently active allocations.
     *
     * This is the difference between total allocations and total deallocations.
     *
     * @return Number of allocations that have not been deallocated (0 if stats disabled).
     */
    uint64_t activeAllocations() const noexcept {
        #ifdef ORTEAF_STATS_LEVEL_CPU_VALUE
            #if ORTEAF_STATS_LEVEL_CPU_VALUE <= 2
                return active_allocations_.load(std::memory_order_relaxed);
            #endif
        #endif
        return 0;
    }
    
    /**
     * @brief Get the current number of allocated bytes.
     *
     * @return Total number of bytes currently allocated (0 if stats disabled or not tracking bytes).
     */
    uint64_t currentAllocatedBytes() const noexcept {
        #ifdef ORTEAF_STATS_LEVEL_CPU_VALUE
            #if ORTEAF_STATS_LEVEL_CPU_VALUE <= 4
                return current_allocated_bytes_.load(std::memory_order_relaxed);
            #endif
        #endif
        return 0;
    }
    
    /**
     * @brief Get the peak number of allocated bytes.
     *
     * This represents the maximum number of bytes that were simultaneously allocated
     * at any point since initialization.
     *
     * @return Peak number of allocated bytes (0 if stats disabled or not tracking bytes).
     */
    uint64_t peakAllocatedBytes() const noexcept {
        #ifdef ORTEAF_STATS_LEVEL_CPU_VALUE
            #if ORTEAF_STATS_LEVEL_CPU_VALUE <= 4
                return peak_allocated_bytes_.load(std::memory_order_relaxed);
            #endif
        #endif
        return 0;
    }
    
    // Update methods (handles both BASIC and EXTENDED levels)
    /**
     * @brief Update statistics when memory is allocated.
     *
     * Updates allocation counts and byte tracking based on the configured statistics level.
     * Thread-safe operation using atomic operations.
     *
     * @param size Size of the allocated memory in bytes.
     */
    void updateAlloc(size_t size) noexcept {
        #ifdef ORTEAF_STATS_LEVEL_CPU_VALUE
            #if ORTEAF_STATS_LEVEL_CPU_VALUE <= 2
                total_allocations_.fetch_add(1, std::memory_order_relaxed);
                active_allocations_.fetch_add(1, std::memory_order_relaxed);
            #endif
            #if ORTEAF_STATS_LEVEL_CPU_VALUE <= 4
                uint64_t current = current_allocated_bytes_.fetch_add(size, std::memory_order_relaxed) + size;
                uint64_t peak = peak_allocated_bytes_.load(std::memory_order_relaxed);
                while (current > peak && !peak_allocated_bytes_.compare_exchange_weak(peak, current, std::memory_order_relaxed)) {
                    peak = peak_allocated_bytes_.load(std::memory_order_relaxed);
                }
            #endif
        #endif
    }
    
    /**
     * @brief Update statistics when memory is deallocated.
     *
     * Updates deallocation counts and byte tracking based on the configured statistics level.
     * Thread-safe operation using atomic operations.
     *
     * @param size Size of the deallocated memory in bytes.
     */
    void updateDealloc(size_t size) noexcept {
        #ifdef ORTEAF_STATS_LEVEL_CPU_VALUE
            #if ORTEAF_STATS_LEVEL_CPU_VALUE <= 2
                total_deallocations_.fetch_add(1, std::memory_order_relaxed);
                active_allocations_.fetch_sub(1, std::memory_order_relaxed);
            #endif
            #if ORTEAF_STATS_LEVEL_CPU_VALUE <= 4
                current_allocated_bytes_.fetch_sub(size, std::memory_order_relaxed);
            #endif
        #endif
    }

    /**
     * @brief Convert statistics to a human-readable string representation.
     *
     * Returns a formatted string containing all available statistics based on
     * the configured statistics level. If statistics are disabled, returns "CPU Stats: Disabled\n".
     *
     * @return String representation of the statistics.
     */
    std::string toString() const {
        std::ostringstream oss;
#if defined(ORTEAF_STATS_LEVEL_CPU_VALUE) && (ORTEAF_STATS_LEVEL_CPU_VALUE <= 4)
        oss << "CPU Stats:\n";
    #if ORTEAF_STATS_LEVEL_CPU_VALUE <= 2
        oss << "  Total Allocations: " << totalAllocations() << "\n";
        oss << "  Total Deallocations: " << totalDeallocations() << "\n";
        oss << "  Active Allocations: " << activeAllocations() << "\n";
    #endif
    #if ORTEAF_STATS_LEVEL_CPU_VALUE <= 4
        oss << "  Current Allocated Bytes: " << currentAllocatedBytes() << "\n";
        oss << "  Peak Allocated Bytes: " << peakAllocatedBytes() << "\n";
    #endif
#else
        oss << "CPU Stats: Disabled\n";
#endif
        return oss.str();
    }
    
    /**
     * @brief Print statistics to standard output.
     *
     * Equivalent to `std::cout << toString()`.
     */
    void print() const {
        std::cout << toString();
    }

private:
#ifdef ORTEAF_STATS_LEVEL_CPU_VALUE
    #if ORTEAF_STATS_LEVEL_CPU_VALUE <= 2
        std::atomic<uint64_t> total_allocations_{0};
        std::atomic<uint64_t> total_deallocations_{0};
        std::atomic<uint64_t> active_allocations_{0};
    #endif
    #if ORTEAF_STATS_LEVEL_CPU_VALUE <= 4
        std::atomic<uint64_t> current_allocated_bytes_{0};
        std::atomic<uint64_t> peak_allocated_bytes_{0};
    #endif
#endif
};

/**
 * @brief Stream output operator for CpuStats.
 *
 * Allows streaming CpuStats objects to output streams.
 *
 * @param os Output stream.
 * @param stats CpuStats object to output.
 * @return Reference to the output stream.
 */
inline std::ostream& operator<<(std::ostream& os, const CpuStats& stats) {
    return os << stats.toString();
}

/**
 * @brief Get the singleton instance of CpuStats.
 *
 * Returns a reference to the global singleton instance of CpuStats.
 * The instance is created on first access and is thread-safe.
 *
 * @return Reference to the global CpuStats instance.
 */
inline CpuStats& statsInstance() {
    static CpuStats stats;
    return stats;
}

/**
 * @brief Get the global CPU statistics instance.
 *
 * Convenience function that returns the same singleton instance as `statsInstance()`.
 *
 * @return Reference to the global CpuStats instance.
 */
inline CpuStats& cpuStats() {
    return statsInstance();
}

/**
 * @brief Update statistics for a memory allocation.
 *
 * Global convenience function that updates the singleton CpuStats instance
 * when memory is allocated.
 *
 * @param size Size of the allocated memory in bytes.
 */
inline void updateAlloc(size_t size) {
    statsInstance().updateAlloc(size);
}

/**
 * @brief Update statistics for a memory deallocation.
 *
 * Global convenience function that updates the singleton CpuStats instance
 * when memory is deallocated.
 *
 * @param size Size of the deallocated memory in bytes.
 */
inline void updateDealloc(size_t size) {
    statsInstance().updateDealloc(size);
}

} // namespace orteaf::internal::backend::cpu
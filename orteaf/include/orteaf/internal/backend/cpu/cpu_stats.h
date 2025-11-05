#pragma once

#include <atomic>

namespace orteaf::internal::backend::cpu {

struct CpuStats {
// STATS_BASIC(2) or STATS_EXTENDED(4) enabled
#ifdef ORTEAF_STATS_LEVEL_CPU_VALUE
    #if ORTEAF_STATS_LEVEL_CPU_VALUE <= 2
        std::atomic<uint64_t> total_allocations{0};
        std::atomic<uint64_t> total_deallocations{0};
        std::atomic<uint64_t> active_allocations{0};
    #endif
    #if ORTEAF_STATS_LEVEL_CPU_VALUE <= 4
        std::atomic<uint64_t> current_allocated_bytes{0};
        std::atomic<uint64_t> peak_allocated_bytes{0};
    #endif
#endif
};

CpuStats cpu_stats();
void update_cpu_stats(CpuStats& stats);
void update_alloc(size_t size);
void update_dealloc(size_t size);
} // namespace orteaf::internal::backend::cpu
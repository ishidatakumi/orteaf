#include "orteaf/internal/backend/cuda/cuda_stats.h"
#include <atomic>

namespace orteaf::internal::backend::cuda {

namespace {
// Single process-wide instance (function-local static ensures one per process and thread-safe init)
inline orteaf::internal::backend::cuda::CUDAStats& stats_inst() {
    static orteaf::internal::backend::cuda::CUDAStats s;
    return s;
}
} // anonymous namespace

CUDAStats get_cuda_stats() {
    return stats_inst();
}

void stats_on_alloc(size_t bytes) {
#if ORTEAF_STATS_LEVEL_CUDA_VALUE == 1
    stats_inst().total_allocations.fetch_add(1, std::memory_order_relaxed);
    stats_inst().active_allocations.fetch_add(1, std::memory_order_relaxed);
#elif ORTEAF_STATS_LEVEL_CUDA_VALUE >= 2
    stats_inst().current_allocated_bytes.fetch_add(bytes, std::memory_order_relaxed);
    const uint64_t current = stats_inst().current_allocated_bytes.load(std::memory_order_relaxed);
    uint64_t prev = stats_inst().peak_allocated_bytes.load(std::memory_order_relaxed);
    while (current > prev &&
           !stats_inst().peak_allocated_bytes.compare_exchange_weak(
               prev, current, std::memory_order_relaxed, std::memory_order_relaxed)) {
        // `prev` is updated with the latest observed value; loop until we set a new peak or observe a taller one
    }
#else
    (void)bytes;
#endif
}

void stats_on_dealloc(size_t bytes) {
#if ORTEAF_STATS_LEVEL_CUDA_VALUE == 1
    stats_inst().total_deallocations.fetch_add(1, std::memory_order_relaxed);
    stats_inst().active_allocations.fetch_sub(1, std::memory_order_relaxed);
#elif ORTEAF_STATS_LEVEL_CUDA_VALUE >= 2
    stats_inst().current_allocated_bytes.fetch_sub(bytes, std::memory_order_relaxed);
#else
    (void)bytes;
#endif
}

void stats_on_create_event() {
#if ORTEAF_STATS_LEVEL_CUDA_VALUE >= 2
    stats_inst().active_events.fetch_add(1, std::memory_order_relaxed);
#endif
}

void stats_on_destroy_event() {
#if ORTEAF_STATS_LEVEL_CUDA_VALUE >= 2
    stats_inst().active_events.fetch_sub(1, std::memory_order_relaxed);
#endif
}

void stats_on_create_stream() {
#if ORTEAF_STATS_LEVEL_CUDA_VALUE >= 2
    stats_inst().active_streams.fetch_add(1, std::memory_order_relaxed);
#endif
}

void stats_on_destroy_stream() {
#if ORTEAF_STATS_LEVEL_CUDA_VALUE >= 2
    stats_inst().active_streams.fetch_sub(1, std::memory_order_relaxed);
#endif
}

void stats_on_device_switch() {
#if ORTEAF_STATS_LEVEL_CUDA_VALUE == 1
    stats_inst().device_switches.fetch_add(1, std::memory_order_relaxed);
#endif
}

void stats_on_active_event() {
#if ORTEAF_STATS_LEVEL_CUDA_VALUE >= 2
    stats_inst().active_events.fetch_add(1, std::memory_order_relaxed);
#endif
}

} // namespace orteaf::internal::backend::cuda
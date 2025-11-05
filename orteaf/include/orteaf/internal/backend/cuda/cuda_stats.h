#pragma once

#include <atomic>

#ifdef CUDA_AVAILABLE
#include <cuda.h>
#endif

namespace orteaf::internal::backend::cuda {
struct CUDAStats {
// 0はoff
#ifdef ORTEAF_STATS_LEVEL_CUDA_VALUE
#if ORTEAF_STATS_LEVEL_CUDA_VALUE <= 2
    std::atomic<uint64_t> total_allocations{0};
    std::atomic<uint64_t> total_deallocations{0};
    std::atomic<uint64_t> active_allocations{0};
    std::atomic<uint64_t> device_switches{0};
#endif
#if ORTEAF_STATS_LEVEL_CUDA_VALUE <= 4
    std::atomic<uint64_t> active_events{0};
    std::atomic<uint64_t> active_streams{0};
    std::atomic<uint64_t> current_allocated_bytes{0};
    std::atomic<uint64_t> peak_allocated_bytes{0};
#endif
#endif
};

CUDAStats get_cuda_stats();
void stats_on_alloc(size_t bytes);
void stats_on_dealloc(size_t bytes);
void stats_on_create_event();
void stats_on_destroy_event();
void stats_on_create_stream();
void stats_on_destroy_stream();
void stats_on_device_switch();
void stats_on_active_event();

} // namespace orteaf::internal::backend::cuda
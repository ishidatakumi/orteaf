#include "orteaf/internal/backend/cuda/cuda_alloc.h"
#include "orteaf/internal/backend/cuda/cuda_check.h"
#include "orteaf/internal/backend/cuda/cuda_stats.h"
#include "orteaf/internal/backend/cuda/cuda_objc_bridge.h"

#ifdef CUDA_AVAILABLE
#include <cuda.h>
#endif

namespace orteaf::internal::backend::cuda {

CUdeviceptr_t alloc(size_t size) {
#ifdef CUDA_AVAILABLE
    CUdeviceptr ptr;
    CU_CHECK(cuMemAlloc(&ptr, size));
    stats_on_alloc(size);
    return opaque_from_cu_deviceptr(ptr);
#else
    (void)size;
    return 0;
#endif
}

void free(CUdeviceptr_t ptr, size_t size) {
#ifdef CUDA_AVAILABLE
    CUdeviceptr objc_ptr = cu_deviceptr_from_opaque(ptr);
    CU_CHECK(cuMemFree(objc_ptr));
    stats_on_dealloc(size);
#else
    (void)ptr;
#endif
}

CUdeviceptr_t alloc_stream(size_t size, CUstream_t stream) {
#ifdef CUDA_AVAILABLE
    CUdeviceptr ptr;
    if (stream == nullptr) {
        // errorで落ちる
        throw std::runtime_error("stream is nullptr");
    }
    CUstream objc_stream = objc_from_opaque_noown<CUstream>(stream);
    CU_CHECK(cuMemAllocAsync(&ptr, size, objc_stream));
    stats_on_alloc(size);
    return opaque_from_cu_deviceptr(ptr);
#else
    (void)size;
    (void)stream;
    return 0;
#endif
}

void free_stream(CUdeviceptr_t ptr, size_t size, CUstream_t stream) {
#ifdef CUDA_AVAILABLE
    CUdeviceptr objc_ptr = cu_deviceptr_from_opaque(ptr);
    if (stream == nullptr) {
        throw std::runtime_error("stream is nullptr");
    }
    CUstream objc_stream = objc_from_opaque_noown<CUstream>(stream);
    CU_CHECK(cuMemFreeAsync(objc_ptr, objc_stream));
    stats_on_dealloc(size);
#else
    (void)ptr;
    (void)stream;
#endif
}

void* alloc_host(std::size_t size) {
#ifdef CUDA_AVAILABLE
    void* ptr;
    CU_CHECK(cuMemAllocHost(&ptr, size));
    stats_on_alloc(size);
    return ptr;
#else
    (void)size;
    return nullptr;
#endif
}

void copy_to_host(CUdeviceptr_t ptr, void* host_ptr, size_t size) {
#ifdef CUDA_AVAILABLE
    CUdeviceptr objc_ptr = cu_deviceptr_from_opaque(ptr);
    CU_CHECK(cuMemcpyDtoH(host_ptr, objc_ptr, size));
    stats_on_alloc(size);
#else
    (void)ptr;
    (void)host_ptr;
    (void)size;
#endif
}

void copy_to_device(void* host_ptr, CUdeviceptr_t ptr, size_t size) {
#ifdef CUDA_AVAILABLE
    CUdeviceptr objc_ptr = cu_deviceptr_from_opaque(ptr);
    CU_CHECK(cuMemcpyHtoD(objc_ptr, host_ptr, size));
    stats_on_alloc(size);
#else
    (void)host_ptr;
    (void)ptr;
    (void)size;
#endif
}

void free_host(void* ptr, size_t size) {
#ifdef CUDA_AVAILABLE
    CU_CHECK(cuMemFreeHost(ptr));
    stats_on_dealloc(size);
#else
    (void)ptr;
#endif
}

} // namespace orteaf::internal::backend::cuda

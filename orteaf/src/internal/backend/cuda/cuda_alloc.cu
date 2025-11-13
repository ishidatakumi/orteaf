#include "orteaf/internal/backend/cuda/cuda_alloc.h"
#include "orteaf/internal/backend/cuda/cuda_check.h"
#include "orteaf/internal/backend/cuda/cuda_stats.h"
#include "orteaf/internal/backend/cuda/cuda_objc_bridge.h"

#include "orteaf/internal/diagnostics/error/error_impl.h"

#ifdef ORTEAF_ENABLE_CUDA
#include <cuda.h>
#endif

namespace orteaf::internal::backend::cuda {

/**
 * @brief Allocate device memory on CUDA device.
 *
 * Implementation of alloc() declared in cuda_alloc.h.
 * Uses cuMemAlloc to allocate device memory and updates statistics.
 *
 * @param size Size of memory to allocate in bytes.
 * @return Opaque CUDA device pointer. Returns 0 if CUDA is not available.
 * @throws std::runtime_error If CUDA allocation fails (when ORTEAF_ENABLE_CUDA is defined).
 */
CUdeviceptr_t alloc(size_t size) {
#ifdef ORTEAF_ENABLE_CUDA
    if (size == 0) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::InvalidParameter, "alloc: size cannot be 0");
    }
    CUdeviceptr ptr;
    CU_CHECK(cuMemAlloc(&ptr, size));
    updateAlloc(size);
    return opaqueFromCuDeviceptr(ptr);
#else
    (void)size;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "alloc: CUDA backend is not available (CUDA disabled)");
#endif
}

/**
 * @brief Free device memory on CUDA device.
 *
 * Implementation of free() declared in cuda_alloc.h.
 * Uses cuMemFree to free device memory and updates statistics.
 *
 * @param ptr Opaque CUDA device pointer to free.
 * @param size Size of memory to free in bytes. Used for statistics update.
 */
void free(CUdeviceptr_t ptr, size_t size) {
#ifdef ORTEAF_ENABLE_CUDA
    if (ptr == 0) return;
    CUdeviceptr objc_ptr = cuDeviceptrFromOpaque(ptr)
    CU_CHECK(cuMemFree(objc_ptr))
    updateDealloc(size);
#else
    (void)ptr;
    (void)size;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "free: CUDA backend is not available (CUDA disabled)");
#endif
}

/**
 * @brief Allocate device memory on CUDA device asynchronously.
 *
 * Implementation of allocStream() declared in cuda_alloc.h.
 * Uses cuMemAllocAsync to allocate device memory asynchronously on the specified stream.
 *
 * @param size Size of memory to allocate in bytes.
 * @param stream CUDA stream handle for asynchronous allocation.
 * @return Opaque CUDA device pointer. Returns 0 if CUDA is not available.
 * @throws std::runtime_error If stream is nullptr or CUDA allocation fails.
 */
CUdeviceptr_t allocStream(size_t size, CUstream_t stream) {
#ifdef ORTEAF_ENABLE_CUDA
    if (size == 0) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::InvalidParameter, "allocStream: size cannot be 0");
    }
    if (stream == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "allocStream: stream cannot be nullptr");
    }
    CUdeviceptr ptr;
    CUstream objc_stream = objcFromOpaqueNoown<CUstream>(stream);
    CU_CHECK(cuMemAllocAsync(&ptr, size, objc_stream));
    updateAlloc(size);
    return opaqueFromCuDeviceptr(ptr);
#else
    (void)size;
    (void)stream;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "allocStream: CUDA backend is not available (CUDA disabled)");
#endif
}

/**
 * @brief Free device memory on CUDA device asynchronously.
 *
 * Implementation of freeStream() declared in cuda_alloc.h.
 * Uses cuMemFreeAsync to free device memory asynchronously on the specified stream.
 *
 * @param ptr Opaque CUDA device pointer to free.
 * @param size Size of memory to free in bytes. Used for statistics update.
 * @param stream CUDA stream handle for asynchronous deallocation.
 * @throws std::runtime_error If stream is nullptr or CUDA deallocation fails.
 */
void freeStream(CUdeviceptr_t ptr, size_t size, CUstream_t stream) {
#ifdef ORTEAF_ENABLE_CUDA
    CUdeviceptr objc_ptr = cuDeviceptrFromOpaque(ptr);
    if (stream == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "freeStream: stream cannot be nullptr");
    }
    CUstream objc_stream = objcFromOpaqueNoown<CUstream>(stream);
    CU_CHECK(cuMemFreeAsync(objc_ptr, objc_stream));
    updateDealloc(size);
#else
    (void)ptr;
    (void)size;
    (void)stream;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "freeStream: CUDA backend is not available (CUDA disabled)");
#endif
}

/**
 * @brief Allocate pinned host memory.
 *
 * Implementation of allocHost() declared in cuda_alloc.h.
 * Uses cuMemAllocHost to allocate page-locked (pinned) host memory.
 *
 * @param size Size of memory to allocate in bytes.
 * @return Pointer to allocated pinned host memory. Returns nullptr if CUDA is not available.
 * @throws std::runtime_error If CUDA allocation fails (when ORTEAF_ENABLE_CUDA is defined).
 */
void* allocHost(std::size_t size) {
#ifdef ORTEAF_ENABLE_CUDA
    if (size == 0) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::InvalidParameter, "allocHost: size cannot be 0");
    }
    void* ptr;
    CU_CHECK(cuMemAllocHost(&ptr, size));
    updateAlloc(size);
    return ptr;
#else
    (void)size;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "allocHost: CUDA backend is not available (CUDA disabled)");
#endif
}

/**
 * @brief Copy data from device to host memory.
 *
 * Implementation of copyToHost() declared in cuda_alloc.h.
 * Uses cuMemcpyDtoH to copy data synchronously from device to host memory.
 * This operation does not allocate memory and does not update statistics.
 *
 * @param ptr Opaque CUDA device pointer to copy from.
 * @param host_ptr Host memory pointer to copy to.
 * @param size Number of bytes to copy.
 * @throws std::runtime_error If CUDA copy operation fails (when ORTEAF_ENABLE_CUDA is defined).
 */
void copyToHost(CUdeviceptr_t ptr, void* host_ptr, size_t size) {
#ifdef ORTEAF_ENABLE_CUDA
    if (ptr == 0) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::InvalidParameter, "copyToHost: ptr cannot be 0");
    }
    if (host_ptr == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "copyToHost: host_ptr cannot be nullptr");
    }
    CUdeviceptr objc_ptr = cuDeviceptrFromOpaque(ptr);
    CU_CHECK(cuMemcpyDtoH(host_ptr, objc_ptr, size));
#else
    (void)ptr;
    (void)host_ptr;
    (void)size;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "copyToHost: CUDA backend is not available (CUDA disabled)");
#endif
}

/**
 * @brief Copy data from host to device memory.
 *
 * Implementation of copyToDevice() declared in cuda_alloc.h.
 * Uses cuMemcpyHtoD to copy data synchronously from host to device memory.
 * This operation does not allocate memory and does not update statistics.
 *
 * @param host_ptr Host memory pointer to copy from.
 * @param ptr Opaque CUDA device pointer to copy to.
 * @param size Number of bytes to copy.
 * @throws std::runtime_error If CUDA copy operation fails (when ORTEAF_ENABLE_CUDA is defined).
 */
void copyToDevice(void* host_ptr, CUdeviceptr_t ptr, size_t size) {
#ifdef ORTEAF_ENABLE_CUDA
    if (ptr == 0) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::InvalidParameter, "copyToDevice: ptr cannot be 0");
    }
    if (host_ptr == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "copyToDevice: host_ptr cannot be nullptr");
    }
    CUdeviceptr objc_ptr = cuDeviceptrFromOpaque(ptr);
    CU_CHECK(cuMemcpyHtoD(objc_ptr, host_ptr, size));
#else
    (void)host_ptr;
    (void)ptr;
    (void)size;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "copyToDevice: CUDA backend is not available (CUDA disabled)");
#endif
}

/**
 * @brief Free pinned host memory.
 *
 * Implementation of freeHost() declared in cuda_alloc.h.
 * Uses cuMemFreeHost to free page-locked (pinned) host memory.
 *
 * @param ptr Pointer to pinned host memory to free.
 * @param size Size of memory to free in bytes. Used for statistics update.
 * @throws std::runtime_error If CUDA deallocation fails (when ORTEAF_ENABLE_CUDA is defined).
 */
void freeHost(void* ptr, size_t size) {
#ifdef ORTEAF_ENABLE_CUDA
    if (ptr == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "freeHost: ptr cannot be nullptr");
    }
    CU_CHECK(cuMemFreeHost(ptr));
    updateDealloc(size);
#else
    (void)ptr;
    (void)size;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "freeHost: CUDA backend is not available (CUDA disabled)");
#endif
}

} // namespace orteaf::internal::backend::cuda

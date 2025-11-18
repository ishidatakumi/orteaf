#pragma once

#if ORTEAF_ENABLE_CUDA

#include <cstddef>
#include "orteaf/internal/backend/cuda/cuda_stream.h"

namespace orteaf::internal::backend::cuda {

/**
 * @brief Allocate device memory on CUDA device.
 *
 * Allocates memory on the CUDA device using cuMemAlloc.
 * Statistics are automatically updated on allocation.
 *
 * @param size Size of memory to allocate in bytes.
 * @return Opaque CUDA device pointer. Returns 0 if CUDA is not available.
 * @throws std::runtime_error If CUDA allocation fails (when ORTEAF_ENABLE_CUDA is defined).
 */
CUdeviceptr_t alloc(size_t size);

/**
 * @brief Free device memory on CUDA device.
 *
 * Frees memory allocated on the CUDA device using cuMemFree.
 * Statistics are automatically updated on deallocation.
 *
 * @param ptr Opaque CUDA device pointer to free.
 * @param size Size of memory to free in bytes. Used for statistics update.
 */
void free(CUdeviceptr_t ptr, size_t size);

/**
 * @brief Allocate device memory on CUDA device asynchronously.
 *
 * Allocates memory on the CUDA device using cuMemAllocAsync.
 * The allocation is performed asynchronously on the specified stream.
 * Statistics are automatically updated on allocation.
 *
 * @param size Size of memory to allocate in bytes.
 * @param stream CUDA stream handle for asynchronous allocation.
 * @return Opaque CUDA device pointer. Returns 0 if CUDA is not available.
 * @throws std::runtime_error If stream is nullptr or CUDA allocation fails.
 */
CUdeviceptr_t allocStream(size_t size, CUstream_t stream);

/**
 * @brief Free device memory on CUDA device asynchronously.
 *
 * Frees memory allocated on the CUDA device using cuMemFreeAsync.
 * The deallocation is performed asynchronously on the specified stream.
 * Statistics are automatically updated on deallocation.
 *
 * @param ptr Opaque CUDA device pointer to free.
 * @param size Size of memory to free in bytes. Used for statistics update.
 * @param stream CUDA stream handle for asynchronous deallocation.
 * @throws std::runtime_error If stream is nullptr or CUDA deallocation fails.
 */
void freeStream(CUdeviceptr_t ptr, size_t size, CUstream_t stream);

/**
 * @brief Allocate pinned host memory.
 *
 * Allocates page-locked (pinned) host memory using cuMemAllocHost.
 * Pinned memory enables faster transfers between host and device.
 * Statistics are automatically updated on allocation.
 *
 * @param size Size of memory to allocate in bytes.
 * @return Pointer to allocated pinned host memory. Returns nullptr if CUDA is not available.
 * @throws std::runtime_error If CUDA allocation fails (when ORTEAF_ENABLE_CUDA is defined).
 */
void* allocHost(size_t size);

/**
 * @brief Copy data from device to host memory.
 *
 * Copies data from CUDA device memory to host memory using cuMemcpyDtoH.
 * This is a synchronous operation.
 * This operation does not allocate memory and does not update statistics.
 *
 * @param ptr Opaque CUDA device pointer to copy from.
 * @param host_ptr Host memory pointer to copy to.
 * @param size Number of bytes to copy.
 * @throws std::runtime_error If CUDA copy operation fails (when ORTEAF_ENABLE_CUDA is defined).
 */
void copyToHost(CUdeviceptr_t ptr, void* host_ptr, size_t size);

/**
 * @brief Copy data from host to device memory.
 *
 * Copies data from host memory to CUDA device memory using cuMemcpyHtoD.
 * This is a synchronous operation.
 * This operation does not allocate memory and does not update statistics.
 *
 * @param host_ptr Host memory pointer to copy from.
 * @param ptr Opaque CUDA device pointer to copy to.
 * @param size Number of bytes to copy.
 * @throws std::runtime_error If CUDA copy operation fails (when ORTEAF_ENABLE_CUDA is defined).
 */
void copyToDevice(void* host_ptr, CUdeviceptr_t ptr, size_t size);

/**
 * @brief Free pinned host memory.
 *
 * Frees page-locked (pinned) host memory allocated by allocHost using cuMemFreeHost.
 * Statistics are automatically updated on deallocation.
 *
 * @param ptr Pointer to pinned host memory to free.
 * @param size Size of memory to free in bytes. Used for statistics update.
 * @throws std::runtime_error If CUDA deallocation fails (when ORTEAF_ENABLE_CUDA is defined).
 */
void freeHost(void* ptr, size_t size);

} // namespace orteaf::internal::backend::cuda

#endif  // ORTEAF_ENABLE_CUDA

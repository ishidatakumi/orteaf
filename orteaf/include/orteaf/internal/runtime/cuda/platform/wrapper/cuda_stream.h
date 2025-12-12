/**
 * @file cuda_stream.h
 * @brief CUDA stream creation, lifetime, synchronization, and memory signaling.
 *
 * Thin wrappers around CUDA Driver API for streams. When CUDA is disabled,
 * functions exist but behave as no-ops and return neutral values.
 */
#pragma once

#if ORTEAF_ENABLE_CUDA

#include "orteaf/internal/runtime/cuda/platform/wrapper/cuda_types.h"

namespace orteaf::internal::runtime::cuda::platform::wrapper {

/**
 * @brief Create a new non-blocking CUDA stream.
 * @return Opaque stream handle, or nullptr when CUDA is disabled.
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 *
 * Also updates internal CUDA statistics on success.
 */
CudaStream_t getStream();

/**
 * @brief Destroy a CUDA stream.
 * @param stream Opaque stream handle; nullptr is ignored.
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 *
 * Also updates internal CUDA statistics on success.
 */
void releaseStream(CudaStream_t stream);

/**
 * @brief Synchronize the given CUDA stream.
 * @param stream Opaque stream handle
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
void synchronizeStream(CudaStream_t stream);

/**
 * @brief Make a stream wait until a device memory value reaches a threshold.
 * @param stream Opaque stream handle
 * @param addr Device memory address (opaque `CudaDevicePtr_t`)
 * @param value Wait until value >= this threshold
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
void waitStream(CudaStream_t stream, CudaDevicePtr_t addr, uint32_t value);

/**
 * @brief Write a 32-bit value to device memory from a stream.
 * @param stream Opaque stream handle
 * @param addr Device memory address (opaque `CudaDevicePtr_t`)
 * @param value Value to write
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
void writeStream(CudaStream_t stream, CudaDevicePtr_t addr, uint32_t value);

} // namespace orteaf::internal::runtime::cuda::platform::wrapper

#endif  // ORTEAF_ENABLE_CUDA

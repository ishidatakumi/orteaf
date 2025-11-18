/**
 * @file cuda_stream.h
 * @brief CUDA stream creation, lifetime, synchronization, and memory signaling.
 *
 * Thin wrappers around CUDA Driver API for streams. When CUDA is disabled,
 * functions exist but behave as no-ops and return neutral values.
 */
#pragma once

#if ORTEAF_ENABLE_CUDA

#include <cstdint>
#include "orteaf/internal/backend/cuda/cuda_device.h"

namespace orteaf::internal::backend::cuda {

struct CUstream_st;
using CUstream_t = CUstream_st*;
using CUdeviceptr_t = std::uint64_t;

static_assert(sizeof(CUdeviceptr_t) == sizeof(std::uint64_t), "CUdeviceptr_t must match 64-bit width.");

// ABI guard: must be pointer-sized on every platform
static_assert(sizeof(CUstream_t) == sizeof(void*), "CUstream_t must be pointer-sized.");

/**
 * @brief Create a new non-blocking CUDA stream.
 * @return Opaque stream handle, or nullptr when CUDA is disabled.
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 *
 * Also updates internal CUDA statistics on success.
 */
CUstream_t getStream();

/**
 * @brief Destroy a CUDA stream.
 * @param stream Opaque stream handle; nullptr is ignored.
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 *
 * Also updates internal CUDA statistics on success.
 */
void releaseStream(CUstream_t stream);

/**
 * @brief Synchronize the given CUDA stream.
 * @param stream Opaque stream handle
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
void synchronizeStream(CUstream_t stream);

/**
 * @brief Make a stream wait until a device memory value reaches a threshold.
 * @param stream Opaque stream handle
 * @param addr Device memory address (opaque `CUdeviceptr_t`)
 * @param value Wait until value >= this threshold
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
void waitStream(CUstream_t stream, CUdeviceptr_t addr, uint32_t value);

/**
 * @brief Write a 32-bit value to device memory from a stream.
 * @param stream Opaque stream handle
 * @param addr Device memory address (opaque `CUdeviceptr_t`)
 * @param value Value to write
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
void writeStream(CUstream_t stream, CUdeviceptr_t addr, uint32_t value);

} // namespace orteaf::internal::backend::cuda

#endif  // ORTEAF_ENABLE_CUDA

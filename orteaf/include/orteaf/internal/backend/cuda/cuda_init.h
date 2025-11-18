/**
 * @file cuda_init.h
 * @brief Initialize the CUDA Driver API once per process.
 *
 * Provides a single entry point to initialize the CUDA driver. Safe to call
 * multiple times; the initialization is performed once in a thread-safe way.
 */
#pragma once

#if ORTEAF_ENABLE_CUDA

namespace orteaf::internal::backend::cuda {

/**
 * @brief Initialize the CUDA driver (idempotent and thread-safe).
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 *
 * Uses an internal one-time guard to ensure `cuInit(0)` runs only once.
 */
void cudaInit();

} // namespace orteaf::internal::backend::cuda

#endif  // ORTEAF_ENABLE_CUDA
/**
 * @file cuda_context.h
 * @brief CUDA context acquisition, creation, activation, and release helpers.
 *
 * Thin wrappers around CUDA Driver API to manage contexts. When CUDA is
 * disabled, functions are defined but behave as no-ops and return nullptr
 * where applicable.
 */
#pragma once

#if ORTEAF_ENABLE_CUDA

#include "orteaf/internal/execution/cuda/platform/wrapper/cuda_types.h"

namespace orteaf::internal::execution::cuda::platform::wrapper {

/**
 * @brief Retain and return the primary context for a device.
 * @param device Opaque device handle (`CudaDevice_t`)
 * @return Opaque primary context (`CudaContext_t`), or nullptr if CUDA disabled
 * @throws std::system_error On CUDA driver error (via `OrteafErrc` mapping)
 */
CudaContext_t getPrimaryContext(CudaDevice_t device);

/**
 * @brief Create a new context for a device.
 * @param device Opaque device handle (`CudaDevice_t`)
 * @return Newly created opaque context (`CudaContext_t`), or nullptr if disabled
 * @throws std::system_error On CUDA driver error (via `OrteafErrc` mapping)
 */
CudaContext_t createContext(CudaDevice_t device);

/**
 * @brief Make the given context current on the calling thread.
 * @param context Opaque context handle (`CudaContext_t`)
 * @throws std::system_error On CUDA driver error (via `OrteafErrc` mapping)
 */
void setContext(CudaContext_t context);

/**
 * @brief Release a device's primary context (decrements retain count).
 * @param device Opaque device handle (`CudaDevice_t`)
 * @throws std::system_error On CUDA driver error (via `OrteafErrc` mapping)
 */
void releasePrimaryContext(CudaDevice_t device);

/**
 * @brief Destroy a context.
 * @param context Opaque context handle (`CudaContext_t`)
 * @throws std::system_error On CUDA driver error (via `OrteafErrc` mapping)
 */
void releaseContext(CudaContext_t context);

} // namespace orteaf::internal::execution::cuda::platform::wrapper

#endif  // ORTEAF_ENABLE_CUDA

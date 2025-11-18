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

#include "orteaf/internal/backend/cuda/cuda_device.h"

namespace orteaf::internal::backend::cuda {

struct CUcontext_st;
using CUcontext_t = CUcontext_st*;

static_assert(sizeof(CUcontext_t) == sizeof(void*), "CUcontext_t must be pointer-sized.");

/**
 * @brief Retain and return the primary context for a device.
 * @param device Opaque device handle (`CUdevice_t`)
 * @return Opaque primary context (`CUcontext_t`), or nullptr if CUDA disabled
 * @throws std::system_error On CUDA driver error (via `OrteafErrc` mapping)
 */
CUcontext_t getPrimaryContext(CUdevice_t device);

/**
 * @brief Create a new context for a device.
 * @param device Opaque device handle (`CUdevice_t`)
 * @return Newly created opaque context (`CUcontext_t`), or nullptr if disabled
 * @throws std::system_error On CUDA driver error (via `OrteafErrc` mapping)
 */
CUcontext_t createContext(CUdevice_t device);

/**
 * @brief Make the given context current on the calling thread.
 * @param context Opaque context handle (`CUcontext_t`)
 * @throws std::system_error On CUDA driver error (via `OrteafErrc` mapping)
 */
void setContext(CUcontext_t context);

/**
 * @brief Release a device's primary context (decrements retain count).
 * @param device Opaque device handle (`CUdevice_t`)
 * @throws std::system_error On CUDA driver error (via `OrteafErrc` mapping)
 */
void releasePrimaryContext(CUdevice_t device);

/**
 * @brief Destroy a context.
 * @param context Opaque context handle (`CUcontext_t`)
 * @throws std::system_error On CUDA driver error (via `OrteafErrc` mapping)
 */
void releaseContext(CUcontext_t context);

} // namespace orteaf::internal::backend::cuda

#endif  // ORTEAF_ENABLE_CUDA
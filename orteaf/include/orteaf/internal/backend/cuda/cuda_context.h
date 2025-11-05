/**
 * @file cuda_context.h
 * @brief CUDA context acquisition, creation, activation, and release helpers.
 *
 * Thin wrappers around CUDA Driver API to manage contexts. When CUDA is
 * disabled, functions are defined but behave as no-ops and return nullptr
 * where applicable.
 */
#pragma once

#include "orteaf/internal/backend/cuda/cuda_device.h"

struct CUcontext_st;
using CUcontext_t = CUcontext_st*;

static_assert(sizeof(CUcontext_t) == sizeof(void*), "CUcontext_t must be pointer-sized.");

namespace orteaf::internal::backend::cuda {

/**
 * @brief Retain and return the primary context for a device.
 * @param device Opaque device handle (`CUdevice_t`)
 * @return Opaque primary context (`CUcontext_t`), or nullptr if CUDA disabled
 * @throws std::system_error On CUDA driver error (via `OrteafErrc` mapping)
 */
CUcontext_t get_primary_context(CUdevice_t device);

/**
 * @brief Create a new context for a device.
 * @param device Opaque device handle (`CUdevice_t`)
 * @return Newly created opaque context (`CUcontext_t`), or nullptr if disabled
 * @throws std::system_error On CUDA driver error (via `OrteafErrc` mapping)
 */
CUcontext_t create_context(CUdevice_t device);

/**
 * @brief Make the given context current on the calling thread.
 * @param context Opaque context handle (`CUcontext_t`)
 * @throws std::system_error On CUDA driver error (via `OrteafErrc` mapping)
 */
void set_context(CUcontext_t context);

/**
 * @brief Release a device's primary context (decrements retain count).
 * @param device Opaque device handle (`CUdevice_t`)
 * @throws std::system_error On CUDA driver error (via `OrteafErrc` mapping)
 */
void release_primary_context(CUdevice_t device);

/**
 * @brief Destroy a context.
 * @param context Opaque context handle (`CUcontext_t`)
 * @throws std::system_error On CUDA driver error (via `OrteafErrc` mapping)
 */
void release_context(CUcontext_t context);

} // namespace orteaf::internal::backend::cuda
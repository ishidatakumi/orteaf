/**
 * @file cuda_module.h
 * @brief CUDA module loading/unloading and function lookup helpers.
 *
 * Thin wrappers around CUDA Driver API for loading PTX/CUBIN/FATBIN modules
 * and retrieving kernel functions. When CUDA is disabled, functions exist but
 * are no-ops and return neutral values (e.g., nullptr).
 */
#pragma once

#if ORTEAF_ENABLE_CUDA

#include "orteaf/internal/execution/cuda/platform/wrapper/cuda_types.h"

namespace orteaf::internal::execution::cuda::platform::wrapper {

/**
 * @brief Load a CUDA module from a file path (PTX/CUBIN/FATBIN supported).
 * @param filepath Path to the module image
 * @return Opaque module handle, or nullptr when CUDA is disabled.
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
CudaModule_t loadModuleFromFile(const char *filepath);

/**
 * @brief Load a CUDA module from an in-memory image (PTX/CUBIN/FATBIN supported).
 * @param image Pointer to the module image in memory
 * @return Opaque module handle, or nullptr when CUDA is disabled.
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
CudaModule_t loadModuleFromImage(const void *image);

/**
 * @brief Retrieve a kernel function handle from a module by name.
 * @param module Opaque module handle
 * @param kernel_name Null-terminated kernel function name
 * @return Opaque function handle, or nullptr when CUDA is disabled.
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
CudaFunction_t getFunction(CudaModule_t module, const char *kernel_name);

/**
 * @brief Unload a CUDA module.
 * @param module Opaque module handle (ignored if nullptr)
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
void unloadModule(CudaModule_t module);

} // namespace orteaf::internal::execution::cuda::platform::wrapper

#endif  // ORTEAF_ENABLE_CUDA

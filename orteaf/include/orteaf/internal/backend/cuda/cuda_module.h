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

namespace orteaf::internal::backend::cuda {

struct CUmodule_st;
using CUmodule_t = CUmodule_st*;

struct CUfunction_st;
using CUfunction_t = CUfunction_st*;

static_assert(sizeof(CUmodule_t) == sizeof(void*), "CUmodule_t must be pointer-sized.");
static_assert(sizeof(CUfunction_t) == sizeof(void*), "CUfunction_t must be pointer-sized.");

/**
 * @brief Load a CUDA module from a file path (PTX/CUBIN/FATBIN supported).
 * @param filepath Path to the module image
 * @return Opaque module handle, or nullptr when CUDA is disabled.
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
CUmodule_t loadModuleFromFile(const char* filepath);

/**
 * @brief Load a CUDA module from an in-memory image (PTX/CUBIN/FATBIN supported).
 * @param image Pointer to the module image in memory
 * @return Opaque module handle, or nullptr when CUDA is disabled.
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
CUmodule_t loadModuleFromImage(const void* image);

/**
 * @brief Retrieve a kernel function handle from a module by name.
 * @param module Opaque module handle
 * @param kernel_name Null-terminated kernel function name
 * @return Opaque function handle, or nullptr when CUDA is disabled.
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
CUfunction_t getFunction(CUmodule_t module, const char* kernel_name);

/**
 * @brief Unload a CUDA module.
 * @param module Opaque module handle (ignored if nullptr)
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
void unloadModule(CUmodule_t module);

} // namespace orteaf::internal::backend::cuda

#endif  // ORTEAF_ENABLE_CUDA

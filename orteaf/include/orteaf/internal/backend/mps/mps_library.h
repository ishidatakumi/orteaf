/**
 * @file mps_library.h
 * @brief MPS/Metal library creation from name, source, or data; destruction.
 */
#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>

#include "orteaf/internal/backend/mps/mps_device.h"
#include "orteaf/internal/backend/mps/mps_string.h"
#include "orteaf/internal/backend/mps/mps_compile_options.h"
#include "orteaf/internal/backend/mps/mps_error.h"

namespace orteaf::internal::backend::mps {

struct MPSLibrary_st; using MPSLibrary_t = MPSLibrary_st*;

static_assert(sizeof(MPSLibrary_t) == sizeof(void*), "MPSLibrary must be pointer-sized.");

/**
 * @brief Create a library by name.
 * @param device Opaque Metal device
 * @param name Opaque string (bridged to NSString*)
 * @param error Optional error out parameter
 * @return Opaque library handle, or nullptr on failure.
 */
[[nodiscard]] MPSLibrary_t createLibrary(MPSDevice_t device, MPSString_t name, MPSError_t* error = nullptr);

/**
 * @brief Create a library from source code.
 * @param device Opaque Metal device
 * @param source Opaque string (bridged to NSString*)
 * @param compile_options Opaque compile options (bridged to MTLCompileOptions*)
 * @param error Optional error out parameter
 * @return Opaque library handle, or nullptr on failure.
 */
[[nodiscard]] MPSLibrary_t createLibraryWithSource(MPSDevice_t device,
                                                      MPSString_t source,
                                                      MPSCompileOptions_t compile_options,
                                                      MPSError_t* error = nullptr);

/**
 * @brief Create a library from binary data.
 * @param device Opaque Metal device
 * @param data Pointer to library data blob
 * @param size Size of library data in bytes
 * @param error Optional error out parameter
 * @return Opaque library handle, or nullptr on failure.
 */
[[nodiscard]] MPSLibrary_t createLibraryWithData(MPSDevice_t device,
                                                    const void* data,
                                                    std::size_t size,
                                                    MPSError_t* error = nullptr);

/**
 * @brief Destroy a library; ignores nullptr.
 */
void destroyLibrary(MPSLibrary_t library);

} // namespace orteaf::internal::backend::mps

#endif  // ORTEAF_ENABLE_MPS

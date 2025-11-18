/**
 * @file mps_function.h
 * @brief MPS/Metal function creation and destruction helpers.
 */
#pragma once

#if ORTEAF_ENABLE_MPS

#include <string_view>

#include "orteaf/internal/backend/mps/mps_library.h"

namespace orteaf::internal::backend::mps {

struct MPSFunction_st; using MPSFunction_t = MPSFunction_st*;

static_assert(sizeof(MPSFunction_t) == sizeof(void*), "MPSFunction must be pointer-sized.");

/**
 * @brief Create a Metal function by name from a library.
 * @param library Opaque library handle
 * @param name Function name (UTF-8)
 * @return Opaque function handle, or nullptr when unavailable/disabled.
 */
MPSFunction_t createFunction(MPSLibrary_t library, std::string_view name);

/**
 * @brief Destroy a Metal function; ignores nullptr.
 */
void destroyFunction(MPSFunction_t function);

} // namespace orteaf::internal::backend::mps

#endif  // ORTEAF_ENABLE_MPS
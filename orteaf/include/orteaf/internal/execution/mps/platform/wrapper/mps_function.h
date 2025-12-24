/**
 * @file mps_function.h
 * @brief MPS/Metal function creation and destruction helpers.
 */
#pragma once

#if ORTEAF_ENABLE_MPS

#include <string_view>

#include "orteaf/internal/execution/mps/platform/wrapper/mps_types.h"

namespace orteaf::internal::execution::mps::platform::wrapper {

/**
 * @brief Create a Metal function by name from a library.
 * @param library Opaque library handle
 * @param name Function name (UTF-8)
 * @return Opaque function handle, or nullptr when unavailable/disabled.
 */
MpsFunction_t createFunction(MpsLibrary_t library, std::string_view name);

/**
 * @brief Destroy a Metal function; ignores nullptr.
 */
void destroyFunction(MpsFunction_t function);

} // namespace orteaf::internal::execution::mps::platform::wrapper

#endif  // ORTEAF_ENABLE_MPS

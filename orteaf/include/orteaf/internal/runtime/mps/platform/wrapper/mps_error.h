/**
 * @file mps_error.h
 * @brief Helpers to construct and destroy NSError-based opaque errors.
 */
#pragma once

#if ORTEAF_ENABLE_MPS

#include <string>
#include <string_view>

#include "orteaf/internal/runtime/mps/platform/wrapper/mps_types.h"

namespace orteaf::internal::runtime::mps::platform::wrapper {

/** Create an error in NSCocoaErrorDomain with a description. */
MpsError_t createError(const std::string &message);
/** Create an error with explicit domain and description. */
MpsError_t createError(std::string_view domain,
                       std::string_view description);
/** Create an error with domain, description, and additional userInfo (NSDictionary*). */
MpsError_t createError(std::string_view domain,
                       std::string_view description,
                       void *additional_user_info);
/** Release an error object; ignores nullptr. */
void destroyError(MpsError_t error);

} // namespace orteaf::internal::runtime::mps::platform::wrapper

#endif  // ORTEAF_ENABLE_MPS

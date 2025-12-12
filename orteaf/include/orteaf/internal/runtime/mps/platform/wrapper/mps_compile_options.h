/**
 * @file mps_compile_options.h
 * @brief MPS/Metal compile options creation and configuration.
 */
#pragma once

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/runtime/mps/platform/wrapper/mps_types.h"

namespace orteaf::internal::runtime::mps::platform::wrapper {

/** Create a new `MTLCompileOptions` object (opaque). */
[[nodiscard]] MpsCompileOptions_t createCompileOptions();

/** Destroy a compile options object; ignores nullptr. */
void destroyCompileOptions(MpsCompileOptions_t options);

/** Set math mode (fast/safe). Requires macOS 15.0 SDK+. */
void setCompileOptionsMathMode(MpsCompileOptions_t options,
                               bool fast_math_enabled);

/** Set preserve invariance flag. */
void setCompileOptionsPreserveInvariance(MpsCompileOptions_t options,
                                         bool preserve_invariance);

/** Set preprocessor macros (expects NSDictionary*). */
void setCompileOptionsPreprocessorMacros(MpsCompileOptions_t options,
                                         void *macros_dictionary);

} // namespace orteaf::internal::runtime::mps::platform::wrapper

#endif  // ORTEAF_ENABLE_MPS

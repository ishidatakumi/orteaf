/**
 * @file mps_compile_options.h
 * @brief MPS/Metal compile options creation and configuration.
 */
#pragma once

#if ORTEAF_ENABLE_MPS

namespace orteaf::internal::backend::mps {

struct MPSCompileOptions_st; using MPSCompileOptions_t = MPSCompileOptions_st*;

static_assert(sizeof(MPSCompileOptions_t) == sizeof(void*), "MPSCompileOptions must be pointer-sized.");

/** Create a new `MTLCompileOptions` object (opaque). */
[[nodiscard]] MPSCompileOptions_t createCompileOptions();

/** Destroy a compile options object; ignores nullptr. */
void destroyCompileOptions(MPSCompileOptions_t options);

/** Set math mode (fast/safe). Requires macOS 15.0 SDK+. */
void setCompileOptionsMathMode(MPSCompileOptions_t options, bool fast_math_enabled);

/** Set preserve invariance flag. */
void setCompileOptionsPreserveInvariance(MPSCompileOptions_t options, bool preserve_invariance);

/** Set preprocessor macros (expects NSDictionary*). */
void setCompileOptionsPreprocessorMacros(MPSCompileOptions_t options, void* macros_dictionary);

} // namespace orteaf::internal::backend::mps

#endif  // ORTEAF_ENABLE_MPS

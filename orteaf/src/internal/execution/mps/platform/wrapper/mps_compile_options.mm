/**
 * @file mps_compile_options.mm
 * @brief Implementation of MPS/Metal compile options helpers.
 */
#ifndef __OBJC__
#error                                                                         \
    "mps_compile_options.mm must be compiled with an Objective-C++ compiler (__OBJC__ not defined)"
#endif
#include "orteaf/internal/execution/mps/platform/wrapper/mps_compile_options.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_objc_bridge.h"

#include "orteaf/internal/diagnostics/error/error.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

namespace orteaf::internal::execution::mps::platform::wrapper {

/**
 * @copydoc orteaf::internal::backend::mps::createCompileOptions
 */
MpsCompileOptions_t createCompileOptions() {
  MTLCompileOptions *options = [[MTLCompileOptions alloc] init];
  return (MpsCompileOptions_t)opaqueFromObjcRetained(options);
}

/**
 * @copydoc orteaf::internal::backend::mps::destroyCompileOptions
 */
void destroyCompileOptions(MpsCompileOptions_t options) {
  if (options == nullptr)
    return;
  opaqueReleaseRetained(options);
}

/**
 * @copydoc orteaf::internal::backend::mps::setCompileOptionsMathMode
 */
void setCompileOptionsMathMode(MpsCompileOptions_t options,
                               bool fast_math_enabled) {
  if (options == nullptr) {
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::NullPointer,
               "setCompileOptionsMathMode: options cannot be nullptr");
  }

  MTLCompileOptions *objc_options =
      objcFromOpaqueNoown<MTLCompileOptions *>(options);
#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) &&                                 \
    __MAC_OS_X_VERSION_MAX_ALLOWED >= 150000
  objc_options.mathMode = fast_math_enabled ? MTLMathModeFast : MTLMathModeSafe;
#else
#error "macOS 15.0 SDK or later is required for MPS support"
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::setCompileOptionsPreserveInvariance
 */
void setCompileOptionsPreserveInvariance(MpsCompileOptions_t options,
                                         bool preserve_invariance) {
  if (options == nullptr) {
    using namespace orteaf::internal::diagnostics::error;
    throwError(
        OrteafErrc::NullPointer,
        "setCompileOptionsPreserveInvariance: options cannot be nullptr");
  }

  MTLCompileOptions *objc_options =
      objcFromOpaqueNoown<MTLCompileOptions *>(options);
  objc_options.preserveInvariance = preserve_invariance;
}

/**
 * @copydoc orteaf::internal::backend::mps::setCompileOptionsPreprocessorMacros
 */
void setCompileOptionsPreprocessorMacros(MpsCompileOptions_t options,
                                         void *macros_dictionary) {
  if (options == nullptr) {
    using namespace orteaf::internal::diagnostics::error;
    throwError(
        OrteafErrc::NullPointer,
        "setCompileOptionsPreprocessorMacros: options cannot be nullptr");
  }

  MTLCompileOptions *objc_options =
      objcFromOpaqueNoown<MTLCompileOptions *>(options);
  NSDictionary *objc_dict =
      objcFromOpaqueNoown<NSDictionary *>(macros_dictionary);
  objc_options.preprocessorMacros = objc_dict;
}

} // namespace orteaf::internal::execution::mps::platform::wrapper

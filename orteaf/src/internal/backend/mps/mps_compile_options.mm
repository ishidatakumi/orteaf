/**
 * @file mps_compile_options.mm
 * @brief Implementation of MPS/Metal compile options helpers.
 */
#ifndef __OBJC__
#error "mps_compile_options.mm must be compiled with an Objective-C++ compiler (__OBJC__ not defined)"
#endif
#include "orteaf/internal/backend/mps/mps_compile_options.h"
#include "orteaf/internal/backend/mps/mps_objc_bridge.h"

#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include "orteaf/internal/diagnostics/error/error.h"
#else
#include "orteaf/internal/diagnostics/error/error_impl.h"
#endif

namespace orteaf::internal::backend::mps {

/**
 * @copydoc orteaf::internal::backend::mps::createCompileOptions
 */
MPSCompileOptions_t createCompileOptions() {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
    return (MPSCompileOptions_t)opaqueFromObjcRetained(options);
#else
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "createCompileOptions: MPS backend is not available (MPS disabled)");
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::destroyCompileOptions
 */
void destroyCompileOptions(MPSCompileOptions_t options) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (options == nullptr) return;
    opaqueReleaseRetained(options);
#else
    (void)options;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "destroyCompileOptions: MPS backend is not available (MPS disabled)");
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::setCompileOptionsMathMode
 */
void setCompileOptionsMathMode(MPSCompileOptions_t options, bool fast_math_enabled) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (options == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "setCompileOptionsMathMode: options cannot be nullptr");
    }
    
    MTLCompileOptions* objc_options = objcFromOpaqueNoown<MTLCompileOptions*>(options);
#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 150000
    objc_options.mathMode = fast_math_enabled ? MTLMathModeFast : MTLMathModeSafe;
#else
#error "macOS 15.0 SDK or later is required for MPS support"
#endif
#else
    (void)options;
    (void)fast_math_enabled;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "setCompileOptionsMathMode: MPS backend is not available (MPS disabled)");
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::setCompileOptionsPreserveInvariance
 */
void setCompileOptionsPreserveInvariance(MPSCompileOptions_t options, bool preserve_invariance) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (options == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "setCompileOptionsPreserveInvariance: options cannot be nullptr");
    }
    
    MTLCompileOptions* objc_options = objcFromOpaqueNoown<MTLCompileOptions*>(options);
    objc_options.preserveInvariance = preserve_invariance;
#else
    (void)options;
    (void)preserve_invariance;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "setCompileOptionsPreserveInvariance: MPS backend is not available (MPS disabled)");
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::setCompileOptionsPreprocessorMacros
 */
void setCompileOptionsPreprocessorMacros(MPSCompileOptions_t options, void* macros_dictionary) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (options == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "setCompileOptionsPreprocessorMacros: options cannot be nullptr");
    }
    
    MTLCompileOptions* objc_options = objcFromOpaqueNoown<MTLCompileOptions*>(options);
    NSDictionary* objc_dict = objcFromOpaqueNoown<NSDictionary*>(macros_dictionary);
    objc_options.preprocessorMacros = objc_dict;
#else
    (void)options;
    (void)macros_dictionary;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "setCompileOptionsPreprocessorMacros: MPS backend is not available (MPS disabled)");
#endif
}

} // namespace orteaf::internal::backend::mps

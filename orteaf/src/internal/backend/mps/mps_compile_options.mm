/**
 * @file mps_compile_options.mm
 * @brief Implementation of MPS/Metal compile options helpers.
 */
#include "orteaf/internal/backend/mps/mps_compile_options.h"
#include "orteaf/internal/backend/mps/mps_objc_bridge.h"

#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include "orteaf/internal/diagnostics/error/error.h"
#endif

namespace orteaf::internal::backend::mps {

/**
 * @copydoc orteaf::internal::backend::mps::create_compile_options
 */
MPSCompileOptions_t create_compile_options() {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
    return (MPSCompileOptions_t)opaque_from_objc_retained(options);
#else
    return nullptr;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::destroy_compile_options
 */
void destroy_compile_options(MPSCompileOptions_t options) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (options != nullptr) {
        opaque_release_retained(options);
    }
#else
    (void)options;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::set_compile_options_math_mode
 */
void set_compile_options_math_mode(MPSCompileOptions_t options, bool fast_math_enabled) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (options == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::NullPointer, "set_compile_options_math_mode: options cannot be nullptr");
    }
    
    MTLCompileOptions* objc_options = objc_from_opaque_noown<MTLCompileOptions*>(options);
#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 150000
    objc_options.mathMode = fast_math_enabled ? MTLMathModeFast : MTLMathModeSafe;
#else
#error "macOS 15.0 SDK or later is required for MPS support"
#endif
#else
    (void)options;
    (void)fast_math_enabled;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::set_compile_options_preserve_invariance
 */
void set_compile_options_preserve_invariance(MPSCompileOptions_t options, bool preserve_invariance) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (options == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::NullPointer, "set_compile_options_preserve_invariance: options cannot be nullptr");
    }
    
    MTLCompileOptions* objc_options = objc_from_opaque_noown<MTLCompileOptions*>(options);
    objc_options.preserveInvariance = preserve_invariance;
#else
    (void)options;
    (void)preserve_invariance;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::set_compile_options_preprocessor_macros
 */
void set_compile_options_preprocessor_macros(MPSCompileOptions_t options, void* macros_dictionary) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (options == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::NullPointer, "set_compile_options_preprocessor_macros: options cannot be nullptr");
    }
    
    MTLCompileOptions* objc_options = objc_from_opaque_noown<MTLCompileOptions*>(options);
    NSDictionary* objc_dict = objc_from_opaque_noown<NSDictionary*>(macros_dictionary);
    objc_options.preprocessorMacros = objc_dict;
#else
    (void)options;
    (void)macros_dictionary;
#endif
}

} // namespace orteaf::internal::backend::mps

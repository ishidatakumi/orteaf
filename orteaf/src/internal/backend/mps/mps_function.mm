/**
 * @file mps_function.mm
 * @brief Implementation of MPS/Metal function helpers.
 */
#include "orteaf/internal/backend/mps/mps_function.h"
#include "orteaf/internal/backend/mps/mps_objc_bridge.h"

#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include "orteaf/internal/diagnostics/error/error.h"
#endif

namespace orteaf::internal::backend::mps {

/**
 * @copydoc orteaf::internal::backend::mps::create_function
 */
MPSFunction_t create_function(MPSLibrary_t library, std::string_view name) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (library == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::NullPointer, "create_function: library cannot be nullptr");
    }
    if (name.empty()) {
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::InvalidParameter, "create_function: name cannot be empty");
    }
    NSString* function_name = [[[NSString alloc] initWithBytes:name.data()
                                                   length:name.size()
                                                 encoding:NSUTF8StringEncoding] autorelease];
    if (function_name == nil) {
        return nullptr;
    }
    id<MTLLibrary> objc_library = objc_from_opaque_noown<id<MTLLibrary>>(library);
    id<MTLFunction> objc_function = [objc_library newFunctionWithName:function_name];
    return (MPSFunction_t)opaque_from_objc_retained(objc_function);
#else
    (void)library;
    (void)name;
    return nullptr;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::destroy_function
 */
void destroy_function(MPSFunction_t function) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (function != nullptr) {
        opaque_release_retained(function);
    }
#else
    (void)function;
#endif
}

} // namespace orteaf::internal::backend::mps
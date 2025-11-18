/**
 * @file mps_function.mm
 * @brief Implementation of MPS/Metal function helpers.
 */
#ifndef __OBJC__
#error "mps_function.mm must be compiled with an Objective-C++ compiler (__OBJC__ not defined)"
#endif
#include "orteaf/internal/backend/mps/mps_function.h"
#include "orteaf/internal/backend/mps/mps_objc_bridge.h"

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::backend::mps {

/**
 * @copydoc orteaf::internal::backend::mps::createFunction
 */
MPSFunction_t createFunction(MPSLibrary_t library, std::string_view name) {
    if (library == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "createFunction: library cannot be nullptr");
    }
    if (name.empty()) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::InvalidParameter, "createFunction: name cannot be empty");
    }
    NSString* function_name = [[[NSString alloc] initWithBytes:name.data()
                                                   length:name.size()
                                                 encoding:NSUTF8StringEncoding] autorelease];
    if (function_name == nil) {
        return nullptr;
    }
    id<MTLLibrary> objc_library = objcFromOpaqueNoown<id<MTLLibrary>>(library);
    id<MTLFunction> objc_function = [objc_library newFunctionWithName:function_name];
    return (MPSFunction_t)opaqueFromObjcRetained(objc_function);
}

/**
 * @copydoc orteaf::internal::backend::mps::destroyFunction
 */
void destroyFunction(MPSFunction_t function) {
    if (function == nullptr) return;
    opaqueReleaseRetained(function);
}

} // namespace orteaf::internal::backend::mps

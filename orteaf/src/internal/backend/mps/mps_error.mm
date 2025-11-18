/**
 * @file mps_error.mm
 * @brief Implementation of NSError construction/destruction helpers.
 */
#ifndef __OBJC__
#error "mps_error.mm must be compiled with an Objective-C++ compiler (__OBJC__ not defined)"
#endif
#include "orteaf/internal/backend/mps/mps_error.h"
#include "orteaf/internal/backend/mps/mps_string.h"
#include "orteaf/internal/backend/mps/mps_objc_bridge.h"

#import <Foundation/Foundation.h>
#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::backend::mps {

namespace {

/** Internal helper to construct NSError with optional userInfo. */
[[nodiscard]] MPSError_t makeError(std::string_view domain,
                                    std::string_view description,
                                    NSDictionary* additional_user_info = nil) {
    MPSString_t domain_string_opaque = toNsString(domain);
    NSString* domain_string = objcFromOpaqueNoown<NSString*>(domain_string_opaque);
    if (domain_string == nil || domain_string.length == 0) {
        domain_string = @"orteaf.mps";
    }

    MPSString_t description_string_opaque = toNsString(description);
    NSString* description_string = objcFromOpaqueNoown<NSString*>(description_string_opaque);
    NSMutableDictionary* user_info = [[[NSMutableDictionary alloc] init] autorelease];
    if (description_string != nil && description_string.length > 0) {
        user_info[NSLocalizedDescriptionKey] = description_string;
    }
    if (additional_user_info != nil) {
        [user_info addEntriesFromDictionary:additional_user_info];
    }

    NSError* objc_error = [[NSError alloc] initWithDomain:domain_string code:0 userInfo:user_info.count > 0 ? user_info : nil];
    return (MPSError_t)opaqueFromObjcRetained(objc_error);
}

} // namespace

/**
 * @copydoc orteaf::internal::backend::mps::createError(const std::string&)
 */
MPSError_t createError(const std::string& message) {
    return makeError("NSCocoaErrorDomain", message);
}

/**
 * @copydoc orteaf::internal::backend::mps::createError(std::string_view,std::string_view)
 */
MPSError_t createError(std::string_view domain, std::string_view description) {
    return makeError(domain, description);
}

/**
 * @copydoc orteaf::internal::backend::mps::createError(std::string_view,std::string_view,void*)
 */
MPSError_t createError(std::string_view domain,
                        std::string_view description,
                        void* additional_user_info) {
    NSDictionary* objc_user_info = objcFromOpaqueNoown<NSDictionary*>(additional_user_info);
    return makeError(domain, description, objc_user_info);
}

/**
 * @copydoc orteaf::internal::backend::mps::destroyError
 */
void destroyError(MPSError_t error) {
    if (error == nullptr) return;
    opaqueReleaseRetained(error);
}

} // namespace orteaf::internal::backend::mps

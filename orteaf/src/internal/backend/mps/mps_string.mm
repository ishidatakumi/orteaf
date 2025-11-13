/**
 * @file mps_string.mm
 * @brief Implementation of NSString conversion helpers.
 */
#ifndef __OBJC__
#error "mps_string.mm must be compiled with an Objective-C++ compiler (__OBJC__ not defined)"
#endif
#include "orteaf/internal/backend/mps/mps_string.h"
#include "orteaf/internal/backend/mps/mps_objc_bridge.h"

#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
#import <Foundation/Foundation.h>
#else
#include "orteaf/internal/diagnostics/error/error_impl.h"
#endif

namespace orteaf::internal::backend::mps {

#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)

/**
 * @copydoc orteaf::internal::backend::mps::toNsString
 */
MPSString_t toNsString(std::string_view view) {
    if (view.empty()) {
        return (MPSString_t)@"";
    }

    NSString* string = [[[NSString alloc] initWithBytes:view.data()
                                              length:view.size()
                                            encoding:NSUTF8StringEncoding] autorelease];
    if (string != nil) {
        return (MPSString_t)string;
    }

    NSString* fallback = [[[NSString alloc] initWithBytes:view.data()
                                                    length:view.size()
                                                  encoding:NSISOLatin1StringEncoding] autorelease];
    return (MPSString_t)fallback;
}

#else // !(defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__))

/** Throw BackendUnavailable on non-ObjC builds or when MPS is disabled. */
MPSString_t toNsString(std::string_view view) {
    (void)view;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "toNsString: MPS backend is not available (MPS disabled)");
}

#endif // defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)

} // namespace orteaf::internal::backend::mps

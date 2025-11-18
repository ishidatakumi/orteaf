/**
 * @file mps_string.mm
 * @brief Implementation of NSString conversion helpers.
 */
#ifndef __OBJC__
#error "mps_string.mm must be compiled with an Objective-C++ compiler (__OBJC__ not defined)"
#endif
#include "orteaf/internal/backend/mps/mps_string.h"
#include "orteaf/internal/backend/mps/mps_objc_bridge.h"

#import <Foundation/Foundation.h>

namespace orteaf::internal::backend::mps {

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

} // namespace orteaf::internal::backend::mps

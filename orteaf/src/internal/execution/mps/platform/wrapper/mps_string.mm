/**
 * @file mps_string.mm
 * @brief Implementation of NSString conversion helpers.
 */
#ifndef __OBJC__
#error                                                                         \
    "mps_string.mm must be compiled with an Objective-C++ compiler (__OBJC__ not defined)"
#endif
#include "orteaf/internal/execution/mps/platform/wrapper/mps_string.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_objc_bridge.h"

#import <Foundation/Foundation.h>

namespace orteaf::internal::execution::mps::platform::wrapper {

/**
 * @copydoc orteaf::internal::backend::mps::toNsString
 */
MpsString_t toNsString(std::string_view view) {
  if (view.empty()) {
    return (MpsString_t) @"";
  }

  NSString *string =
      [[[NSString alloc] initWithBytes:view.data()
                                length:view.size()
                              encoding:NSUTF8StringEncoding] autorelease];
  if (string != nil) {
    return (MpsString_t)string;
  }

  NSString *fallback =
      [[[NSString alloc] initWithBytes:view.data()
                                length:view.size()
                              encoding:NSISOLatin1StringEncoding] autorelease];
  return (MpsString_t)fallback;
}

} // namespace orteaf::internal::execution::mps::platform::wrapper

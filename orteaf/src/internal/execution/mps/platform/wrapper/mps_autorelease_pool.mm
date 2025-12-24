/**
 * @file mps_autorelease_pool.mm
 * @brief Implementation of NSAutoreleasePool RAII wrapper.
 */
#ifndef __OBJC__
#error "mps_autorelease_pool.mm must be compiled with an Objective-C++ compiler (__OBJC__ not defined)"
#endif
#include "orteaf/internal/execution/mps/platform/wrapper/mps_autorelease_pool.h"

#import <Foundation/Foundation.h>
#include <chrono>
#include <iostream>

namespace orteaf::internal::execution::mps::platform::wrapper {

/** Construct a new autorelease pool. */
AutoreleasePool::AutoreleasePool() {
#if __has_feature(objc_arc)
    // ARCでもNSAutoreleasePoolは使える（推奨は@autoreleasepoolだが、ラッパー内なら可）
    pool_ = (NSAutoreleasePool*)[[NSAutoreleasePool alloc] init];
#else
    pool_ = [[NSAutoreleasePool alloc] init];
#endif
}

/** Drain and destroy the pool on scope exit. */
AutoreleasePool::~AutoreleasePool() {
    if (pool_) {
#if __has_feature(objc_arc)
        [(id)pool_ drain];
#else
        [pool_ drain];
#endif
        pool_ = nullptr;
    }
}

} // namespace orteaf::internal::execution::mps::platform::wrapper



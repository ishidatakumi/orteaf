/**
 * @file mps_objc_bridge.h
 * @brief Objective-C pointer <-> opaque handle bridging helpers.
 *
 * This header must be included only from Objective-C++ (.mm) files.
 * Functions use ARC-aware bridging to retain/release when requested.
 */
#pragma once

#if ORTEAF_ENABLE_MPS

#ifndef __OBJC__
#  error "This header must be included only from Objective-C++ (.mm)"
#endif
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>

/** Raw cast without changing ownership. */
template <typename ObjcPtr>
static inline void* opaqueFromObjcNoown(ObjcPtr p) noexcept {
    return (__bridge void*)p;
}
template <typename ObjcPtr>
static inline ObjcPtr objcFromOpaqueNoown(void* p) noexcept {
    return (__bridge ObjcPtr)p;
}

/** Return an opaque handle with +1 retain (ARC-aware). */
template <typename ObjcPtr>
static inline void* opaqueFromObjcRetained(ObjcPtr p) noexcept {
#if __has_feature(objc_arc)
    return (__bridge_retained void*)p;
#else
    [p retain];
    return (__bridge void*)p;
#endif
}

/** Release a previously retained opaque handle. */
static inline void opaqueReleaseRetained(void* p) noexcept {
#if __has_feature(objc_arc)
    CFBridgingRelease(p);
#else
    [(id)p release];
#endif
}

/** Retain an opaque handle and return it. */
static inline void* opaqueRetain(void* p) noexcept {
#if __has_feature(objc_arc)
    CFRetain(p);
#else
    [(id)p retain];
#endif
    return p;
}

#endif  // ORTEAF_ENABLE_MPS

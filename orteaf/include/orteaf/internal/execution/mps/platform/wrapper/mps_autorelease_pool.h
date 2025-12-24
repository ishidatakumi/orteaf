/**
 * @file mps_autorelease_pool.h
 * @brief RAII wrapper for NSAutoreleasePool with an easy macro.
 */
#pragma once

#if ORTEAF_ENABLE_MPS

#if defined(__OBJC__)
#import <Foundation/Foundation.h>
#else
using NSAutoreleasePool = void;
#endif

namespace orteaf::internal::execution::mps::platform::wrapper {

class AutoreleasePool {
public:
    /** Create a new autorelease pool. */
    AutoreleasePool();
    /** Drain and destroy the autorelease pool. */
    ~AutoreleasePool();

    AutoreleasePool(const AutoreleasePool&) = delete;
    AutoreleasePool& operator=(const AutoreleasePool&) = delete;
    AutoreleasePool(AutoreleasePool&&) = delete;
    AutoreleasePool& operator=(AutoreleasePool&&) = delete;

private:
    NSAutoreleasePool* pool_;
};

} // namespace orteaf::internal::execution::mps::platform::wrapper

#define _BITS_CONCAT2(a,b) a##b
#define _BITS_CONCAT(a,b)  _BITS_CONCAT2(a,b)
/**
 * @def MPS_AUTORELEASE_POOL()
 * @brief Declare a scoped autorelease pool instance.
 */
#define MPS_AUTORELEASE_POOL() \
    orteaf::internal::execution::mps::platform::wrapper::AutoreleasePool _BITS_CONCAT(_orteaf_mps_autorelease_, __LINE__)

#else  // ORTEAF_ENABLE_MPS

namespace orteaf::internal::execution::mps::platform::wrapper {

class AutoreleasePool {
public:
    AutoreleasePool() = default;
    ~AutoreleasePool() = default;
};

} // namespace orteaf::internal::execution::mps::platform::wrapper

/** No-op macro when MPS is disabled. */
#define MPS_AUTORELEASE_POOL()

#endif  // ORTEAF_ENABLE_MPS



/**
 * @file mps_size.h
 * @brief POD size type and conversions for MPS/Metal.
 */
#pragma once

#include <cstdint>

#if ORTEAF_ENABLE_MPS

#if defined(__OBJC__)
#import <Metal/Metal.h>
#endif

namespace orteaf::internal::runtime::mps::platform::wrapper {

using MpsInt_t = int64_t; // 8 bytes size
using MPSInt_t = MpsInt_t;

/**
 * @brief 3D integer size (width/height/depth), ABI-stable 24 bytes.
 */
struct MpsSize_st {
    MpsInt_t width;
    MpsInt_t height;
    MpsInt_t depth;

    char padding[24 - 3 * sizeof(MpsInt_t)];
};

using MpsSize_t = MpsSize_st;
using MPSSize_t = MpsSize_t;

static_assert(sizeof(MpsSize_t) == 24, "MpsSize must be 24 bytes.");
static_assert(sizeof(MpsInt_t) == 8, "MpsInt must be 8 bytes.");

/** Construct an `MPSSize_t` from components. */
MpsSize_t makeSize(MpsInt_t width, MpsInt_t height, MpsInt_t depth);

#if defined(__OBJC__)
/** Convert to `MTLSize`. */
MTLSize toMtlSize(MpsSize_t size);
/** Convert from `MTLSize`. */
MpsSize_t fromMtlSize(MTLSize mtl_size);
#endif  // defined(__OBJC__)

} // namespace orteaf::internal::runtime::mps::platform::wrapper

#endif  // ORTEAF_ENABLE_MPS

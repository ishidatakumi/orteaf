/**
 * @file mps_buffer.mm
 * @brief Implementation of MPS/Metal buffer helpers.
 */
#ifndef __OBJC__
#error "mps_buffer.mm must be compiled with an Objective-C++ compiler (__OBJC__ not defined)"
#endif
#include "orteaf/internal/backend/mps/mps_buffer.h"
#include "orteaf/internal/backend/mps/mps_heap.h"
#include "orteaf/internal/backend/mps/mps_objc_bridge.h"

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::backend::mps {

/**
 * @copydoc orteaf::internal::backend::mps::createBuffer
 */
MPSBuffer_t createBuffer(MPSHeap_t heap, size_t size, MPSBufferUsage_t usage) {
    if (heap == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "createBuffer: heap cannot be nullptr");
    }
    if (size == 0) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::InvalidParameter, "createBuffer: size cannot be 0");
    }
    id<MTLHeap> objc_heap = objcFromOpaqueNoown<id<MTLHeap>>(heap);
    MTLResourceOptions objc_usage = static_cast<MTLResourceOptions>(usage);
    id<MTLBuffer> objc_buffer = [objc_heap newBufferWithLength:size options:objc_usage];
    if (objc_buffer == nil) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::OperationFailed, "createBuffer: heap allocation failed");
    }
    return (MPSBuffer_t)opaqueFromObjcRetained(objc_buffer);
}

/**
 * @copydoc orteaf::internal::backend::mps::destroyBuffer
 */
void destroyBuffer(MPSBuffer_t buffer) {
    if (buffer == nullptr) return;
    opaqueReleaseRetained(buffer);
}

/**
 * @copydoc orteaf::internal::backend::mps::getBufferContentsConst
 */
const void* getBufferContentsConst(MPSBuffer_t buffer) {
    if (buffer == nullptr) return nullptr;
    id<MTLBuffer> objc_buffer = objcFromOpaqueNoown<id<MTLBuffer>>(buffer);
    return [objc_buffer contents];
}

/**
 * @copydoc orteaf::internal::backend::mps::getBufferContents
 */
void* getBufferContents(MPSBuffer_t buffer) {
    return const_cast<void*>(getBufferContentsConst(buffer));
}
} // namespace orteaf::internal::backend::mps

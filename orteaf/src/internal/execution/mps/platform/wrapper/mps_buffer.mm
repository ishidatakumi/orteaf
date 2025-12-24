/**
 * @file mps_buffer.mm
 * @brief Implementation of MPS/Metal buffer helpers.
 */
#ifndef __OBJC__
#error                                                                         \
    "mps_buffer.mm must be compiled with an Objective-C++ compiler (__OBJC__ not defined)"
#endif
#include "orteaf/internal/execution/mps/platform/wrapper/mps_buffer.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_heap.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_objc_bridge.h"

#include "orteaf/internal/diagnostics/error/error.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

namespace orteaf::internal::execution::mps::platform::wrapper {

/**
 * @copydoc orteaf::internal::backend::mps::createBuffer
 */
MpsBuffer_t createBuffer(MpsHeap_t heap, size_t size, MpsBufferUsage_t usage) {
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
  id<MTLBuffer> objc_buffer = [objc_heap newBufferWithLength:size
                                                     options:objc_usage];
  if (objc_buffer == nil) {
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::OperationFailed,
               "createBuffer: heap allocation failed");
  }
  return (MpsBuffer_t)opaqueFromObjcRetained(objc_buffer);
}

MpsBuffer_t createBufferWithOffset(MpsHeap_t heap, size_t size, size_t offset,
                                   MpsBufferUsage_t usage) {
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

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 110000
  if ([objc_heap respondsToSelector:@selector(newBufferWithLength:
                                                          options:offset:)]) {
    id<MTLBuffer> objc_buffer = [objc_heap newBufferWithLength:size
                                                       options:objc_usage
                                                        offset:offset];
    if (objc_buffer == nil) {
      using namespace orteaf::internal::diagnostics::error;
      throwError(OrteafErrc::OperationFailed,
                 "createBuffer: heap allocation failed");
    }
    return (MpsBuffer_t)opaqueFromObjcRetained(objc_buffer);
  }
#endif

  // Fallback: no offset support on this platform; attempt regular allocation
  id<MTLBuffer> objc_buffer = [objc_heap newBufferWithLength:size
                                                     options:objc_usage];
  if (objc_buffer == nil) {
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::OperationFailed,
               "createBuffer (offset): heap allocation failed");
  }
  return (MpsBuffer_t)opaqueFromObjcRetained(objc_buffer);
}

/**
 * @copydoc orteaf::internal::backend::mps::destroyBuffer
 */
void destroyBuffer(MpsBuffer_t buffer) {
  if (buffer == nullptr)
    return;
  opaqueReleaseRetained(buffer);
}

/**
 * @copydoc orteaf::internal::backend::mps::getBufferContentsConst
 */
const void *getBufferContentsConst(MpsBuffer_t buffer) {
  if (buffer == nullptr)
    return nullptr;
  id<MTLBuffer> objc_buffer = objcFromOpaqueNoown<id<MTLBuffer>>(buffer);
  return [objc_buffer contents];
}

/**
 * @copydoc orteaf::internal::backend::mps::getBufferContents
 */
void *getBufferContents(MpsBuffer_t buffer) {
  return const_cast<void *>(getBufferContentsConst(buffer));
}
} // namespace orteaf::internal::execution::mps::platform::wrapper

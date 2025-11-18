/**
 * @file mps_command_buffer.mm
 * @brief Implementation of MPS/Metal command buffer helpers.
 */
#ifndef __OBJC__
#error "mps_command_buffer.mm must be compiled with an Objective-C++ compiler (__OBJC__ not defined)"
#endif
#include "orteaf/internal/backend/mps/mps_command_buffer.h"
#include "orteaf/internal/backend/mps/mps_command_queue.h"
#include "orteaf/internal/backend/mps/mps_event.h"
#include "orteaf/internal/backend/mps/mps_autorelease_pool.h"
#include "orteaf/internal/backend/mps/mps_objc_bridge.h"

#import <Metal/Metal.h>
#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::backend::mps {

using orteaf::internal::backend::mps::AutoreleasePool;

/**
 * @copydoc orteaf::internal::backend::mps::createCommandBuffer
 */
MPSCommandBuffer_t createCommandBuffer(MPSCommandQueue_t command_queue) {
    if (command_queue == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "createCommandBuffer: command_queue cannot be nullptr");
    }
    AutoreleasePool pool{};
    id<MTLCommandQueue> objc_command_queue = objcFromOpaqueNoown<id<MTLCommandQueue>>(command_queue);
    id<MTLCommandBuffer> objc_command_buffer = [objc_command_queue commandBuffer];
    return (MPSCommandBuffer_t)opaqueFromObjcRetained(objc_command_buffer);
}

/**
 * @copydoc orteaf::internal::backend::mps::destroyCommandBuffer
 */
void destroyCommandBuffer(MPSCommandBuffer_t command_buffer) {
    if (command_buffer == nullptr) return;
    AutoreleasePool pool{};
    opaqueReleaseRetained(command_buffer);
}

/**
 * @copydoc orteaf::internal::backend::mps::encodeSignalEvent
 */
void encodeSignalEvent(MPSCommandBuffer_t command_buffer, MPSEvent_t event, uint32_t value) {
    if (command_buffer == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "encodeSignalEvent: command_buffer cannot be nullptr");
    }
    if (event == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "encodeSignalEvent: event cannot be nullptr");
    }
    id<MTLCommandBuffer> objc_command_buffer = objcFromOpaqueNoown<id<MTLCommandBuffer>>(command_buffer);
    id<MTLSharedEvent> objc_event = objcFromOpaqueNoown<id<MTLSharedEvent>>(event);
    [objc_command_buffer encodeSignalEvent:objc_event value:value];
}

/**
 * @copydoc orteaf::internal::backend::mps::encodeWait
 */
void encodeWait(MPSCommandBuffer_t command_buffer, MPSEvent_t event, uint32_t value) {
    if (command_buffer == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "encodeWait: command_buffer cannot be nullptr");
    }
    if (event == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "encodeWait: event cannot be nullptr");
    }
    id<MTLCommandBuffer> objc_command_buffer = objcFromOpaqueNoown<id<MTLCommandBuffer>>(command_buffer);
    id<MTLSharedEvent> objc_event = objcFromOpaqueNoown<id<MTLSharedEvent>>(event);
    [objc_command_buffer encodeWaitForEvent:objc_event value:value];
}

/**
 * @copydoc orteaf::internal::backend::mps::commit
 */
void commit(MPSCommandBuffer_t command_buffer) {
    if (command_buffer == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "commit: command_buffer cannot be nullptr");
    }
    id<MTLCommandBuffer> objc_command_buffer = objcFromOpaqueNoown<id<MTLCommandBuffer>>(command_buffer);
    [objc_command_buffer commit];
}

/**
 * @copydoc orteaf::internal::backend::mps::waitUntilCompleted
 */
void waitUntilCompleted(MPSCommandBuffer_t command_buffer) {
    if (command_buffer == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "waitUntilCompleted: command_buffer cannot be nullptr");
    }
    id<MTLCommandBuffer> objc_command_buffer = objcFromOpaqueNoown<id<MTLCommandBuffer>>(command_buffer);
    [objc_command_buffer waitUntilCompleted];
}

} // namespace orteaf::internal::backend::mps

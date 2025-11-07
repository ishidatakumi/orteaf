/**
 * @file mps_command_buffer.mm
 * @brief Implementation of MPS/Metal command buffer helpers.
 */
#include "orteaf/internal/backend/mps/mps_command_buffer.h"
#include "orteaf/internal/backend/mps/mps_command_queue.h"
#include "orteaf/internal/backend/mps/mps_event.h"
#include "orteaf/internal/backend/mps/mps_autorelease_pool.h"
#include "orteaf/internal/backend/mps/mps_objc_bridge.h"

#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
#import <Metal/Metal.h>
#include "orteaf/internal/diagnostics/error/error.h"
#endif

namespace orteaf::internal::backend::mps {

using orteaf::internal::backend::mps::AutoreleasePool;

/**
 * @copydoc orteaf::internal::backend::mps::create_command_buffer
 */
MPSCommandBuffer_t create_command_buffer(MPSCommandQueue_t command_queue) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (command_queue == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::NullPointer, "create_command_buffer: command_queue cannot be nullptr");
    }
    AutoreleasePool pool{};
    id<MTLCommandQueue> objc_command_queue = objc_from_opaque_noown<id<MTLCommandQueue>>(command_queue);
    id<MTLCommandBuffer> objc_command_buffer = [objc_command_queue commandBuffer];
    return (MPSCommandBuffer_t)opaque_from_objc_retained(objc_command_buffer);
#else
    (void)command_queue;
    return nullptr;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::destroy_command_buffer
 */
void destroy_command_buffer(MPSCommandBuffer_t command_buffer) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (command_buffer != nullptr) {
        AutoreleasePool pool{};
        opaque_release_retained(command_buffer);
    }
#else
    (void)command_buffer;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::encode_signal_event
 */
void encode_signal_event(MPSCommandBuffer_t command_buffer, MPSEvent_t event, uint32_t value) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (command_buffer == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::NullPointer, "encode_signal_event: command_buffer cannot be nullptr");
    }
    if (event == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::NullPointer, "encode_signal_event: event cannot be nullptr");
    }
    id<MTLCommandBuffer> objc_command_buffer = objc_from_opaque_noown<id<MTLCommandBuffer>>(command_buffer);
    id<MTLSharedEvent> objc_event = objc_from_opaque_noown<id<MTLSharedEvent>>(event);
    [objc_command_buffer encodeSignalEvent:objc_event value:value];
#else
    (void)command_buffer;
    (void)event;
    (void)value;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::encode_wait
 */
void encode_wait(MPSCommandBuffer_t command_buffer, MPSEvent_t event, uint32_t value) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (command_buffer == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::NullPointer, "encode_wait: command_buffer cannot be nullptr");
    }
    if (event == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::NullPointer, "encode_wait: event cannot be nullptr");
    }
    id<MTLCommandBuffer> objc_command_buffer = objc_from_opaque_noown<id<MTLCommandBuffer>>(command_buffer);
    id<MTLSharedEvent> objc_event = objc_from_opaque_noown<id<MTLSharedEvent>>(event);
    [objc_command_buffer encodeWaitForEvent:objc_event value:value];
#else
    (void)command_buffer;
    (void)event;
    (void)value;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::commit
 */
void commit(MPSCommandBuffer_t command_buffer) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (command_buffer == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::NullPointer, "commit: command_buffer cannot be nullptr");
    }
    id<MTLCommandBuffer> objc_command_buffer = objc_from_opaque_noown<id<MTLCommandBuffer>>(command_buffer);
    [objc_command_buffer commit];
#else
    (void)command_buffer;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::wait_until_completed
 */
void wait_until_completed(MPSCommandBuffer_t command_buffer) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (command_buffer == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::NullPointer, "wait_until_completed: command_buffer cannot be nullptr");
    }
    id<MTLCommandBuffer> objc_command_buffer = objc_from_opaque_noown<id<MTLCommandBuffer>>(command_buffer);
    [objc_command_buffer waitUntilCompleted];
#else
    (void)command_buffer;
#endif
}

} // namespace orteaf::internal::backend::mps

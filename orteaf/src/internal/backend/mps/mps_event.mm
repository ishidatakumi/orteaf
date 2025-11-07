/**
 * @file mps_event.mm
 * @brief Implementation of MPS/Metal shared event helpers.
 */
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

#include "orteaf/internal/backend/mps/mps_event.h"
#include "orteaf/internal/backend/mps/mps_autorelease_pool.h"
#include "orteaf/internal/backend/mps/mps_command_buffer.h"
#include "orteaf/internal/backend/mps/mps_stats.h"
#include "orteaf/internal/backend/mps/mps_objc_bridge.h"
#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::backend::mps {

/**
 * @copydoc orteaf::internal::backend::mps::create_event
 */
MPSEvent_t create_event(MPSDevice_t device) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    orteaf::internal::backend::mps::AutoreleasePool pool{};
    id<MTLDevice> objc_device = objc_from_opaque_noown<id<MTLDevice>>(device);
    id<MTLSharedEvent> objc_event = [objc_device newSharedEvent];
    if (!objc_event) {
        return nullptr;
    }
    [objc_event setSignaledValue:0];
    update_create_event();
    auto retained = opaque_from_objc_retained(objc_event);
    return (MPSEvent_t)retained;
#else
    (void)device;
    return nullptr;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::destroy_event
 */
void destroy_event(MPSEvent_t event) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (!event) return;
    orteaf::internal::backend::mps::AutoreleasePool pool{};
    opaque_release_retained(event);
    update_destroy_event();
#else
    (void)event;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::record_event
 */
void record_event(MPSEvent_t event, MPSCommandBuffer_t command_buffer, uint64_t value) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (!event) return;
    id<MTLSharedEvent> objc_event = objc_from_opaque_noown<id<MTLSharedEvent>>(event);
    if (command_buffer) {
        id<MTLCommandBuffer> objc_command_buffer = objc_from_opaque_noown<id<MTLCommandBuffer>>(command_buffer);
        [objc_command_buffer encodeSignalEvent:objc_event value:value];
    } else {
        [objc_event setSignaledValue:value];
    }
#else
    (void)event;
    (void)command_buffer;
    (void)value;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::query_event
 */
bool query_event(MPSEvent_t event, uint64_t expected_value) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (!event) return true;
    id<MTLSharedEvent> objc_event = objc_from_opaque_noown<id<MTLSharedEvent>>(event);
    return [objc_event signaledValue] >= expected_value;
#else
    (void)event;
    (void)expected_value;
    return true;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::event_value
 */
uint64_t event_value(MPSEvent_t event) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (!event) return 0;
    id<MTLSharedEvent> e = objc_from_opaque_noown<id<MTLSharedEvent>>(event);
    return [e signaledValue];
#else
    (void)event; return 0;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::wait_event
 */
void wait_event(MPSCommandBuffer_t command_buffer, MPSEvent_t event, uint64_t value) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (!command_buffer || !event) return;
    id<MTLCommandBuffer> objc_command_buffer = objc_from_opaque_noown<id<MTLCommandBuffer>>(command_buffer);
    id<MTLSharedEvent> objc_event = objc_from_opaque_noown<id<MTLSharedEvent>>(event);
    [objc_command_buffer encodeWaitForEvent:objc_event value:value];
#else
    (void)command_buffer;
    (void)event;
    (void)value;
#endif
}

} // namespace orteaf::internal::backend::mps

#endif // defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)

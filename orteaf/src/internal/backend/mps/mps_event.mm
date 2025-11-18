/**
 * @file mps_event.mm
 * @brief Implementation of MPS/Metal shared event helpers.
 */
#ifndef __OBJC__
#error "mps_event.mm must be compiled with an Objective-C++ compiler (__OBJC__ not defined)"
#endif
#include "orteaf/internal/backend/mps/mps_event.h"

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

#include "orteaf/internal/backend/mps/mps_autorelease_pool.h"
#include "orteaf/internal/backend/mps/mps_command_buffer.h"
#include "orteaf/internal/backend/mps/mps_stats.h"
#include "orteaf/internal/backend/mps/mps_objc_bridge.h"
#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::backend::mps {

/**
 * @copydoc orteaf::internal::backend::mps::createEvent
 */
MPSEvent_t createEvent(MPSDevice_t device) {
    orteaf::internal::backend::mps::AutoreleasePool pool{};
    id<MTLDevice> objc_device = objcFromOpaqueNoown<id<MTLDevice>>(device);
    id<MTLSharedEvent> objc_event = [objc_device newSharedEvent];
    if (objc_event == nil) {
        return nullptr;
    }
    [objc_event setSignaledValue:0];
    updateCreateEvent();
    auto retained = opaqueFromObjcRetained(objc_event);
    return (MPSEvent_t)retained;
}

/**
 * @copydoc orteaf::internal::backend::mps::destroyEvent
 */
void destroyEvent(MPSEvent_t event) {
    if (event == nullptr) return;
    orteaf::internal::backend::mps::AutoreleasePool pool{};
    opaqueReleaseRetained(event);
    updateDestroyEvent();
}

/**
 * @copydoc orteaf::internal::backend::mps::recordEvent
 */
void recordEvent(MPSEvent_t event, MPSCommandBuffer_t command_buffer, uint64_t value) {
    if (event == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "recordEvent: event cannot be nullptr");
    }
    id<MTLSharedEvent> objc_event = objcFromOpaqueNoown<id<MTLSharedEvent>>(event);
    if (command_buffer) {
        id<MTLCommandBuffer> objc_command_buffer = objcFromOpaqueNoown<id<MTLCommandBuffer>>(command_buffer);
        [objc_command_buffer encodeSignalEvent:objc_event value:value];
    } else {
        [objc_event setSignaledValue:value];
    }
}

/**
 * @copydoc orteaf::internal::backend::mps::queryEvent
 */
bool queryEvent(MPSEvent_t event, uint64_t expected_value) {
    if (event == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "queryEvent: event cannot be nullptr");
    }
    id<MTLSharedEvent> objc_event = objcFromOpaqueNoown<id<MTLSharedEvent>>(event);
    return [objc_event signaledValue] >= expected_value;
}

/**
 * @copydoc orteaf::internal::backend::mps::eventValue
 */
uint64_t eventValue(MPSEvent_t event) {
    if (event == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "eventValue: event cannot be nullptr");
    }
    id<MTLSharedEvent> e = objcFromOpaqueNoown<id<MTLSharedEvent>>(event);
    return [e signaledValue];
}

/**
 * @copydoc orteaf::internal::backend::mps::waitEvent
 */
void waitEvent(MPSCommandBuffer_t command_buffer, MPSEvent_t event, uint64_t value) {
    if (command_buffer == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "waitEvent: command_buffer cannot be nullptr");
    }
    if (event == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "waitEvent: event cannot be nullptr");
    }
    id<MTLCommandBuffer> objc_command_buffer = objcFromOpaqueNoown<id<MTLCommandBuffer>>(command_buffer);
    id<MTLSharedEvent> objc_event = objcFromOpaqueNoown<id<MTLSharedEvent>>(event);
    [objc_command_buffer encodeWaitForEvent:objc_event value:value];
}

} // namespace orteaf::internal::backend::mps

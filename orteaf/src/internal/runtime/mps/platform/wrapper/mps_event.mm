/**
 * @file mps_event.mm
 * @brief Implementation of MPS/Metal shared event helpers.
 */
#ifndef __OBJC__
#error                                                                         \
    "mps_event.mm must be compiled with an Objective-C++ compiler (__OBJC__ not defined)"
#endif
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_event.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_autorelease_pool.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_command_buffer.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_objc_bridge.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_stats.h"

namespace orteaf::internal::runtime::mps::platform::wrapper {

/**
 * @copydoc orteaf::internal::backend::mps::createEvent
 */
MpsEvent_t createEvent(MpsDevice_t device) {
  ::orteaf::internal::runtime::mps::platform::wrapper::AutoreleasePool pool{};
  id<MTLDevice> objc_device = objcFromOpaqueNoown<id<MTLDevice>>(device);
  id<MTLSharedEvent> objc_event = [objc_device newSharedEvent];
  if (objc_event == nil) {
    return nullptr;
  }
  [objc_event setSignaledValue:0];
  updateCreateEvent();
  auto retained = opaqueFromObjcRetained(objc_event);
  return (MpsEvent_t)retained;
}

/**
 * @copydoc orteaf::internal::backend::mps::destroyEvent
 */
void destroyEvent(MpsEvent_t event) {
  if (event == nullptr)
    return;
  ::orteaf::internal::runtime::mps::platform::wrapper::AutoreleasePool pool{};
  opaqueReleaseRetained(event);
  updateDestroyEvent();
}

/**
 * @copydoc orteaf::internal::backend::mps::recordEvent
 */
void recordEvent(MpsEvent_t event, MpsCommandBuffer_t command_buffer,
                 uint64_t value) {
  if (event == nullptr) {
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::NullPointer, "recordEvent: event cannot be nullptr");
  }
  id<MTLSharedEvent> objc_event =
      objcFromOpaqueNoown<id<MTLSharedEvent>>(event);
  if (command_buffer) {
    id<MTLCommandBuffer> objc_command_buffer =
        objcFromOpaqueNoown<id<MTLCommandBuffer>>(command_buffer);
    [objc_command_buffer encodeSignalEvent:objc_event value:value];
  } else {
    [objc_event setSignaledValue:value];
  }
}

/**
 * @copydoc orteaf::internal::backend::mps::queryEvent
 */
bool queryEvent(MpsEvent_t event, uint64_t expected_value) {
  if (event == nullptr) {
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::NullPointer, "queryEvent: event cannot be nullptr");
  }
  id<MTLSharedEvent> objc_event =
      objcFromOpaqueNoown<id<MTLSharedEvent>>(event);
  return [objc_event signaledValue] >= expected_value;
}

/**
 * @copydoc orteaf::internal::backend::mps::eventValue
 */
uint64_t eventValue(MpsEvent_t event) {
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
void waitEvent(MpsCommandBuffer_t command_buffer, MpsEvent_t event,
               uint64_t value) {
  if (command_buffer == nullptr) {
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::NullPointer,
               "waitEvent: command_buffer cannot be nullptr");
  }
  if (event == nullptr) {
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::NullPointer, "waitEvent: event cannot be nullptr");
  }
  id<MTLCommandBuffer> objc_command_buffer =
      objcFromOpaqueNoown<id<MTLCommandBuffer>>(command_buffer);
  id<MTLSharedEvent> objc_event =
      objcFromOpaqueNoown<id<MTLSharedEvent>>(event);
  [objc_command_buffer encodeWaitForEvent:objc_event value:value];
}

} // namespace orteaf::internal::runtime::mps::platform::wrapper

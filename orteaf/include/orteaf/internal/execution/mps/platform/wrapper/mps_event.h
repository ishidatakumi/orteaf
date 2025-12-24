/**
 * @file mps_event.h
 * @brief MPS/Metal shared event helpers (create/destroy/record/wait/query).
 */
#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstdint>

#include "orteaf/internal/execution/mps/platform/wrapper/mps_types.h"

namespace orteaf::internal::execution::mps::platform::wrapper {

/** Create a shared event for a device (initial value = 0). */
MpsEvent_t createEvent(MpsDevice_t device);
/** Destroy a shared event; ignores nullptr. */
void destroyEvent(MpsEvent_t event);
/** Record/signal event from a command buffer; or set directly when null. */
void recordEvent(MpsEvent_t event, MpsCommandBuffer_t command_buffer,
                 uint64_t value = 1);
/** Check if event's signaledValue >= expected_value. */
bool queryEvent(MpsEvent_t event, uint64_t expected_value = 1);
/** Get current signaledValue for the event. */
uint64_t eventValue(MpsEvent_t event);
/** Encode wait in a command buffer until event reaches value. */
void waitEvent(MpsCommandBuffer_t command_buffer, MpsEvent_t event,
               uint64_t value = 1);

} // namespace orteaf::internal::execution::mps::platform::wrapper

#endif  // ORTEAF_ENABLE_MPS

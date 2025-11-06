/**
 * @file mps_event.h
 * @brief MPS/Metal shared event helpers (create/destroy/record/wait/query).
 */
#pragma once

#include "orteaf/internal/backend/mps/mps_device.h"
#include "orteaf/internal/backend/mps/mps_command_queue.h"

namespace orteaf::internal::backend::mps {

struct MPSEvent_st;
struct MPSCommandBuffer_st;
using MPSEvent_t = MPSEvent_st*;
using MPSCommandBuffer_t = MPSCommandBuffer_st*;

static_assert(sizeof(MPSCommandBuffer_t) == sizeof(void*), "MPSCommandBuffer must be pointer-sized.");
static_assert(sizeof(MPSEvent_t) == sizeof(void*), "MPSEvent_t must be pointer-sized.");

/** Create a shared event for a device (initial value = 0). */
MPSEvent_t create_event(MPSDevice_t device);
/** Destroy a shared event; ignores nullptr. */
void destroy_event(MPSEvent_t event);
/** Record/signal event from a command buffer; or set directly when null. */
void record_event(MPSEvent_t event, MPSCommandBuffer_t command_buffer, uint64_t value = 1);
/** Check if event's signaledValue >= expected_value. */
bool query_event(MPSEvent_t event, uint64_t expected_value = 1);
/** Get current signaledValue for the event. */
uint64_t event_value(MPSEvent_t event);
/** Encode wait in a command buffer until event reaches value. */
void wait_event(MPSCommandBuffer_t command_buffer, MPSEvent_t event, uint64_t value = 1);

} // namespace orteaf::internal::backend::mps


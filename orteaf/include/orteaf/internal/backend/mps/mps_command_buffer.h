/**
 * @file mps_command_buffer.h
 * @brief MPS/Metal command buffer creation and submission helpers.
 */
#pragma once

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/backend/mps/mps_command_queue.h"
#include "orteaf/internal/backend/mps/mps_event.h"

namespace orteaf::internal::backend::mps {

/**
 * @brief Create a command buffer from a command queue.
 * @return Opaque command buffer handle, or nullptr when unavailable/disabled.
 */
MPSCommandBuffer_t createCommandBuffer(MPSCommandQueue_t command_queue);

/**
 * @brief Destroy a command buffer.
 */
void destroyCommandBuffer(MPSCommandBuffer_t command_buffer);

/**
 * @brief Encode a signal to an event with the given value.
 */
void encodeSignalEvent(MPSCommandBuffer_t command_buffer, MPSEvent_t event, uint32_t value);

/**
 * @brief Encode a wait on an event until it reaches the given value.
 */
void encodeWait(MPSCommandBuffer_t command_buffer, MPSEvent_t event, uint32_t value);

/**
 * @brief Commit the command buffer for execution.
 */
void commit(MPSCommandBuffer_t command_buffer);

/**
 * @brief Block until the command buffer has completed execution.
 */
void waitUntilCompleted(MPSCommandBuffer_t command_buffer);

} // namespace orteaf::internal::backend::mps

#endif  // ORTEAF_ENABLE_MPS
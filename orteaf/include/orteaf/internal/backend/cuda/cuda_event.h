/**
 * @file cuda_event.h
 * @brief CUDA event creation, lifetime, and synchronization helpers.
 *
 * Provides thin wrappers around CUDA Driver API for events. When CUDA is
 * disabled, functions are available but behave as no-ops and return
 * neutral values (e.g., nullptr or true) where applicable.
 */
#pragma once

#if ORTEAF_ENABLE_CUDA

#include "orteaf/internal/backend/cuda/cuda_stream.h"

namespace orteaf::internal::backend::cuda {

struct CUevent_st;
using CUevent_t = CUevent_st*;

static_assert(sizeof(CUevent_t) == sizeof(void*), "CUevent_t must be pointer-sized.");

/**
 * @brief Create a CUDA event with timing disabled.
 * @return Opaque event handle, or nullptr when CUDA is disabled.
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 *
 * Also updates internal CUDA statistics on success.
 */
CUevent_t createEvent();

/**
 * @brief Destroy a CUDA event.
 * @param event Opaque event handle; nullptr is ignored.
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 *
 * Also updates internal CUDA statistics on success.
 */
void destroyEvent(CUevent_t event);

/**
 * @brief Record an event into a stream.
 * @param event Opaque event handle; nullptr is ignored.
 * @param stream Opaque stream handle; nullptr is ignored.
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
void recordEvent(CUevent_t event, CUstream_t stream);

/**
 * @brief Query whether an event has completed.
 * @param event Opaque event handle; nullptr is treated as already complete.
 * @return true if completed or CUDA disabled; false if not ready.
 * @throws std::system_error On unexpected CUDA driver error (via `OrteafErrc`).
 */
bool queryEvent(CUevent_t event);

/**
 * @brief Make a stream wait for an event to complete.
 * @param stream Opaque stream handle; nullptr is ignored.
 * @param event Opaque event handle; nullptr is ignored.
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
void waitEvent(CUstream_t stream, CUevent_t event);

} // namespace orteaf::internal::backend::cuda

#endif  // ORTEAF_ENABLE_CUDA
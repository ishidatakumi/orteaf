#pragma once

struct CUevent_st;
using CUevent_t = CUevent_st*;

#include "orteaf/internal/backend/cuda/cuda_stream.h"

static_assert(sizeof(CUevent_t) == sizeof(void*), "CUevent_t must be pointer-sized.");

namespace orteaf::internal::backend::cuda {
CUevent_t create_event();
void destroy_event(CUevent_t event);
void record_event(CUevent_t event, CUstream_t stream);
bool query_event(CUevent_t event);
void wait_event(CUstream_t stream, CUevent_t event);
} // namespace orteaf::internal::backend::cuda
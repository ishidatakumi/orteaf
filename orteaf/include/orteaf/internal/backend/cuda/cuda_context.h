#pragma once

#include "orteaf/internal/backend/cuda/cuda_device.h"

struct CUcontext_st;
using CUcontext_t = CUcontext_st*;

static_assert(sizeof(CUcontext_t) == sizeof(void*), "CUcontext_t must be pointer-sized.");

namespace orteaf::internal::backend::cuda {

CUcontext_t get_primary_context(CUdevice_t device);
CUcontext_t create_context(CUdevice_t device);
void set_context(CUcontext_t context);
void release_primary_context(CUdevice_t device);
void release_context(CUcontext_t context);

} // namespace orteaf::internal::backend::cuda
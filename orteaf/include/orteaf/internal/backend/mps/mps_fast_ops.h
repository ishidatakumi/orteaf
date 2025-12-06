#pragma once

#include "orteaf/internal/backend/mps/wrapper/mps_command_buffer.h"

namespace orteaf::internal::runtime::backend_ops::mps {

struct MpsFastOps {
  // Fast-path wrapper for command buffer creation.
  static inline ::orteaf::internal::backend::mps::MPSCommandBuffer_t createCommandBuffer(
      ::orteaf::internal::backend::mps::MPSCommandQueue_t command_queue) {
    return ::orteaf::internal::backend::mps::createCommandBuffer(command_queue);
  }
};

} // namespace orteaf::internal::runtime::backend_ops::mps

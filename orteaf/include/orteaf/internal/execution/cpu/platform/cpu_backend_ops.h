#pragma once

#include "orteaf/internal/architecture/architecture.h"
#include "orteaf/internal/architecture/cpu_detect.h"

namespace orteaf::internal::execution::cpu::platform {

/**
 * @brief Concrete backend operations for the host CPU.
 *
 * The struct simply forwards to lower-level detection helpers. Device manager
 * templates can substitute this type with mocks during testing to validate
 * failure modes without relying on the actual hardware.
 */
struct CpuBackendOps {
  static ::orteaf::internal::architecture::Architecture detectArchitecture() {
    return ::orteaf::internal::architecture::detectCpuArchitecture();
  }
};

} // namespace orteaf::internal::execution::cpu::platform

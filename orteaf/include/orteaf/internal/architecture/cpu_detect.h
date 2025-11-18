#pragma once

#include "orteaf/internal/architecture/architecture.h"

namespace orteaf::internal::architecture {

/**
 * @brief Match the host CPU against the canonical architecture table.
 *
 * The detector gathers platform information (vendor string, CPUID family/model, machine
 * identifiers, and feature flags) and compares that data against the generated
 * architecture metadata. When a concrete CPU entry matches the collected signals, that
 * entry is returned; otherwise the result falls back to `Architecture::cpu_generic`.
 *
 * @return The best matching CPU `Architecture` or `Architecture::cpu_generic`.
 */
Architecture detectCpuArchitecture();

} // namespace orteaf::internal::architecture

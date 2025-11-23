#pragma once

#include "orteaf/internal/architecture/architecture.h"
#include "orteaf/internal/base/strong_id.h"

#include <cstdint>
#include <string_view>

namespace orteaf::internal::architecture {

/**
 * @brief Select the CUDA architecture that matches the provided compute capability.
 *
 * The generated architecture metadata keeps compute capability hints per entry, so the
 * detector walks each CUDA backend row and returns the first match that shares the requested
 * capability and (optionally) vendor. Undefined capabilities (negative values) skip the check,
 * allowing the generic architecture to be returned instead.
 *
 * @param compute_capability Numeric compute capability expressed as `major*10 + minor` (e.g., 80 for SM80).
 * @param vendor_hint Optional vendor hint, defaults to `"nvidia"`, that filters tables with stricter matches.
 * @return The matching CUDA `Architecture`, or `Architecture::CudaGeneric` when no table entry matches.
 */
Architecture detectCudaArchitecture(int compute_capability, std::string_view vendor_hint = "nvidia");

/**
 * @brief Enumerate CUDA devices and detect the architecture for the requested `DeviceId`.
 *
 * When CUDA is enabled, the helper queries `backend::cuda` for the device count, extracts
 * the compute capability and vendor for the requested device, and then delegates to
 * `detectCudaArchitecture`. Out-of-range devices or missing CUDA support result in
 * `Architecture::CudaGeneric`.
 *
 * @param device_id Strong-typed CUDA device identifier.
 * @return The detected CUDA `Architecture`, or `Architecture::CudaGeneric` when enumeration fails.
 */
Architecture detectCudaArchitectureForDeviceId(::orteaf::internal::base::DeviceId device_id);

} // namespace orteaf::internal::architecture

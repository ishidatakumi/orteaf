#pragma once

#include "orteaf/internal/architecture/architecture.h"
#include "orteaf/internal/base/strong_id.h"

#include <cstdint>
#include <string_view>

namespace orteaf::internal::architecture {

/**
 * @brief Determine the MPS (Apple Metal) architecture from the reported Metal family string.
 *
 * The generated architecture metadata includes Metal family hints such as `"m3"` or `"m4"`,
 * which are matched against the provided string. Vendor hints can further narrow the results,
 * e.g., differentiating between Apple and third-party MPS implementations. When no match is
 * found, the detector returns `Architecture::mps_generic`.
 *
 * @param metal_family The reported Metal family (typically `"m3"`/`"m4"`).
 * @param vendor_hint Optional vendor name, defaults to `"apple"`, used to match metadata rows.
 * @return The detected MPS `Architecture`, or `Architecture::mps_generic` when no suitable entry exists.
 */
Architecture detectMpsArchitecture(std::string_view metal_family, std::string_view vendor_hint = "apple");

/**
 * @brief Enumerate MPS devices and detect the architecture for the requested `DeviceId`.
 *
 * When MPS is enabled, the helper queries `backend::mps` for the device count, grabs the Metal
 * family and vendor strings for the requested device, and delegates to `detectMpsArchitecture`.
 * Invalid device IDs or disabled MPS support fall back to `Architecture::mps_generic`.
 *
 * @param device_id Strong-typed MPS device identifier.
 * @return The detected MPS `Architecture`, or `Architecture::mps_generic` on failure.
 */
Architecture detectMpsArchitectureForDeviceId(::orteaf::internal::base::DeviceId device_id);

} // namespace orteaf::internal::architecture

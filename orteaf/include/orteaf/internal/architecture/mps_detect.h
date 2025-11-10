#pragma once

#include "orteaf/internal/architecture/architecture.h"

#include <cstdint>
#include <string_view>

namespace orteaf::internal::architecture {

/// Detect the MPS (Metal) architecture using the reported Metal family (e.g., "m3") and optional vendor hint.
Architecture detect_mps_architecture(std::string_view metal_family, std::string_view vendor_hint = "apple");

/// Detect the architecture by enumerating the MPS backend (out-of-range -> generic).
Architecture detect_mps_architecture_for_device_index(std::uint32_t device_index);

} // namespace orteaf::internal::architecture

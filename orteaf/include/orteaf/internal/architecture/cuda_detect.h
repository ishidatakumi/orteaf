#pragma once

#include "orteaf/internal/architecture/architecture.h"

#include <cstdint>
#include <string_view>

namespace orteaf::internal::architecture {

/// Detect the CUDA architecture given a compute capability (e.g., 80 for SM80) and optional vendor hint.
Architecture detect_cuda_architecture(int compute_capability, std::string_view vendor_hint = "nvidia");

/// Detect using real device info by enumerating the CUDA backend (out-of-range -> generic).
Architecture detect_cuda_architecture_for_device_index(std::uint32_t device_index);

} // namespace orteaf::internal::architecture

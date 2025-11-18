#include "orteaf/internal/architecture/cuda_detect.h"

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/backend/cuda/cuda_device.h"
#include "orteaf/internal/diagnostics/error/error.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <system_error>

namespace orteaf::internal::architecture {

namespace diagnostics = ::orteaf::internal::diagnostics;

namespace {

namespace tables = ::orteaf::generated::architecture_tables;

std::string toLowerCopy(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return result;
}

/**
 * @brief Compare optional vendor expectations against the normalized hint.
 *
 * When metadata leaves the vendor empty, any hint is acceptable.
 */
bool matchesVendor(std::string_view required, std::string_view actual_lower) {
    if (required.empty()) {
        return true;
    }
    return toLowerCopy(required) == actual_lower;
}

} // namespace

/**
 * @copydoc orteaf::internal::architecture::detectCudaArchitecture
 */
Architecture detectCudaArchitecture(int compute_capability, std::string_view vendor_hint) {
    const auto vendor_lower = toLowerCopy(vendor_hint);
    const auto count = tables::kArchitectureCount;
    Architecture fallback = Architecture::cuda_generic;

    for (std::size_t index = 0; index < count; ++index) {
        const Architecture arch = kAllArchitectures[index];
        if (localIndexOf(arch) == 0) {
            continue;
        }
        if (backendOf(arch) != backend::Backend::cuda) {
            continue;
        }

        const auto required_vendor = tables::kArchitectureDetectVendors[index];
        if (!matchesVendor(required_vendor, vendor_lower)) {
            continue;
        }

        const int required_cc = tables::kArchitectureDetectComputeCapability[index];
        if (required_cc >= 0 && required_cc != compute_capability) {
            continue;
        }

        return arch;
    }

    return fallback;
}

/**
 * @copydoc orteaf::internal::architecture::detectCudaArchitectureForDeviceId
 */
Architecture detectCudaArchitectureForDeviceId(::orteaf::internal::base::DeviceId device_id) {
#if ORTEAF_ENABLE_CUDA
    using backend::cuda::ComputeCapability;
    using backend::cuda::CUdevice_t;
    using diagnostics::error::OrteafErrc;

    const std::uint32_t device_index = static_cast<std::uint32_t>(device_id);
    const auto backend_unavailable = diagnostics::error::makeErrorCode(OrteafErrc::BackendUnavailable);

    try {
        int count = backend::cuda::getDeviceCount();
        if (count <= 0 || device_index >= static_cast<std::uint32_t>(count)) {
            return Architecture::cuda_generic;
        }

        CUdevice_t device = backend::cuda::getDevice(device_index);
        if (!device) {
            return Architecture::cuda_generic;
        }

        ComputeCapability capability = backend::cuda::getComputeCapability(device);
        const int cc_value = capability.major * 10 + capability.minor;
        std::string vendor = backend::cuda::getDeviceVendor(device);
        if (vendor.empty()) {
            vendor = "nvidia";
        }
        return detectCudaArchitecture(cc_value, vendor);
    } catch (const std::system_error& err) {
        if (err.code() == backend_unavailable) {
            // CUDA driver not ready on this environment; pretend it's generic.
            return Architecture::cuda_generic;
        }
        throw;
    }
#else
    (void)device_id;
    return Architecture::cuda_generic;
#endif
}

} // namespace orteaf::internal::architecture

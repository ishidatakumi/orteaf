#include "orteaf/internal/architecture/mps_detect.h"

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/diagnostics/error/error.h"

#if ORTEAF_ENABLE_MPS
#include "orteaf/internal/backend/mps/mps_device.h"
#endif

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
 * @brief Compare optional vendor requirements against the normalized hint.
 */
bool matchesVendor(std::string_view required, std::string_view hint_lower) {
    if (required.empty()) {
        return true;
    }
    return toLowerCopy(required) == hint_lower;
}

#if ORTEAF_ENABLE_MPS
class ScopedDevice {
public:
    explicit ScopedDevice(backend::mps::MPSDevice_t device) : device_(device) {}
    ScopedDevice(const ScopedDevice&) = delete;
    ScopedDevice& operator=(const ScopedDevice&) = delete;
    ~ScopedDevice() {
        if (device_ != nullptr) {
            backend::mps::deviceRelease(device_);
        }
    }

    backend::mps::MPSDevice_t get() const { return device_; }

private:
    backend::mps::MPSDevice_t device_;
};
#endif // ORTEAF_ENABLE_MPS

} // namespace

/**
 * @copydoc orteaf::internal::architecture::detectMpsArchitecture
 */
Architecture detectMpsArchitecture(std::string_view metal_family, std::string_view vendor_hint) {
    const auto metal_lower = toLowerCopy(metal_family);
    const auto vendor_lower = toLowerCopy(vendor_hint);
    const auto count = tables::kArchitectureCount;
    Architecture fallback = Architecture::mps_generic;

    for (std::size_t index = 0; index < count; ++index) {
        const Architecture arch = kAllArchitectures[index];
        if (localIndexOf(arch) == 0) {
            continue;
        }
        if (backendOf(arch) != backend::Backend::mps) {
            continue;
        }

        const auto required_vendor = tables::kArchitectureDetectVendors[index];
        if (!matchesVendor(required_vendor, vendor_lower)) {
            continue;
        }

        const auto required_family = tables::kArchitectureDetectMetalFamilies[index];
        if (!required_family.empty() && toLowerCopy(required_family) != metal_lower) {
            continue;
        }

        return arch;
    }

    return fallback;
}

/**
 * @copydoc orteaf::internal::architecture::detectMpsArchitectureForDeviceId
 */
Architecture detectMpsArchitectureForDeviceId(::orteaf::internal::base::DeviceId device_id) {
#if ORTEAF_ENABLE_MPS
    const std::uint32_t device_index = static_cast<std::uint32_t>(device_id);
    const auto backend_unavailable = diagnostics::error::makeErrorCode(
        diagnostics::error::OrteafErrc::BackendUnavailable);

    try {
        int count = backend::mps::getDeviceCount();
        if (count <= 0 || device_index >= static_cast<std::uint32_t>(count)) {
            return Architecture::mps_generic;
        }

        backend::mps::MPSDevice_t device =
            backend::mps::getDevice(static_cast<backend::mps::MPSInt_t>(device_index));
        if (device == nullptr) {
            return Architecture::mps_generic;
        }

        ScopedDevice guard(device);
        std::string metal_family = backend::mps::getDeviceMetalFamily(device);
        std::string vendor = backend::mps::getDeviceVendor(device);
        if (vendor.empty()) {
            vendor = "apple";
        }
        return detectMpsArchitecture(metal_family, vendor);
    } catch (const std::system_error& err) {
        if (err.code() == backend_unavailable) {
            return Architecture::mps_generic;
        }
        throw;
    }
#else
    (void)device_id;
    return Architecture::mps_generic;
#endif
}

} // namespace orteaf::internal::architecture

#include "orteaf/internal/architecture/mps_detect.h"

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/backend/mps/mps_device.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace orteaf::internal::architecture {

namespace {

namespace tables = ::orteaf::generated::architecture_tables;

std::string ToLowerCopy(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return result;
}

bool MatchesVendor(std::string_view required, std::string_view hint_lower) {
    if (required.empty()) {
        return true;
    }
    return ToLowerCopy(required) == hint_lower;
}

class ScopedDevice {
public:
    explicit ScopedDevice(backend::mps::MPSDevice_t device) : device_(device) {}
    ScopedDevice(const ScopedDevice&) = delete;
    ScopedDevice& operator=(const ScopedDevice&) = delete;
    ~ScopedDevice() {
        if (device_ != nullptr) {
            backend::mps::device_release(device_);
        }
    }

    backend::mps::MPSDevice_t get() const { return device_; }

private:
    backend::mps::MPSDevice_t device_;
};

} // namespace

Architecture detect_mps_architecture(std::string_view metal_family, std::string_view vendor_hint) {
    const auto metal_lower = ToLowerCopy(metal_family);
    const auto vendor_lower = ToLowerCopy(vendor_hint);
    const auto count = tables::kArchitectureCount;
    Architecture fallback = Architecture::mps_generic;

    for (std::size_t index = 0; index < count; ++index) {
        const Architecture arch = kAllArchitectures[index];
        if (LocalIndexOf(arch) == 0) {
            continue;
        }
        if (BackendOf(arch) != backend::Backend::mps) {
            continue;
        }

        const auto required_vendor = tables::kArchitectureDetectVendors[index];
        if (!MatchesVendor(required_vendor, vendor_lower)) {
            continue;
        }

        const auto required_family = tables::kArchitectureDetectMetalFamilies[index];
        if (!required_family.empty() && ToLowerCopy(required_family) != metal_lower) {
            continue;
        }

        return arch;
    }

    return fallback;
}

Architecture detect_mps_architecture_for_device_index(std::uint32_t device_index) {
#if ORTEAF_ENABLE_MPS
    int count = backend::mps::get_device_count();
    if (count <= 0 || device_index >= static_cast<std::uint32_t>(count)) {
        return Architecture::mps_generic;
    }

    backend::mps::MPSDevice_t device = backend::mps::get_device(static_cast<backend::mps::MPSInt_t>(device_index));
    if (device == nullptr) {
        return Architecture::mps_generic;
    }

    ScopedDevice guard(device);
    std::string metal_family = backend::mps::get_device_metal_family(device);
    std::string vendor = backend::mps::get_device_vendor(device);
    if (vendor.empty()) {
        vendor = "apple";
    }
    return detect_mps_architecture(metal_family, vendor);
#else
    (void)device_index;
    return Architecture::mps_generic;
#endif
}

} // namespace orteaf::internal::architecture

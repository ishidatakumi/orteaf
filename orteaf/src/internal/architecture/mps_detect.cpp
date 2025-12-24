#include "orteaf/internal/architecture/mps_detect.h"

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/diagnostics/error/error.h"

#if ORTEAF_ENABLE_MPS
#include "orteaf/internal/execution/mps/platform/wrapper/mps_device.h"
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
  std::transform(
      result.begin(), result.end(), result.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
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
  explicit ScopedDevice(
      ::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t device)
      : device_(device) {}
  ScopedDevice(const ScopedDevice &) = delete;
  ScopedDevice &operator=(const ScopedDevice &) = delete;
  ~ScopedDevice() {
    if (device_ != nullptr) {
      ::orteaf::internal::execution::mps::platform::wrapper::deviceRelease(
          device_);
    }
  }

  ::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t get() const {
    return device_;
  }

private:
  ::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t device_;
};
#endif // ORTEAF_ENABLE_MPS

} // namespace

/**
 * @copydoc orteaf::internal::architecture::detectMpsArchitecture
 */
Architecture detectMpsArchitecture(std::string_view metal_family,
                                   std::string_view vendor_hint) {
  const auto metal_lower = toLowerCopy(metal_family);
  const auto vendor_lower = toLowerCopy(vendor_hint);
  const auto count = tables::kArchitectureCount;
  Architecture fallback = Architecture::MpsGeneric;

  for (std::size_t index = 0; index < count; ++index) {
    const Architecture arch = kAllArchitectures[index];
    if (localIndexOf(arch) == 0) {
      continue;
    }
    if (backendOf(arch) != backend::Backend::Mps) {
      continue;
    }

    const auto required_vendor = tables::kArchitectureDetectVendors[index];
    if (!matchesVendor(required_vendor, vendor_lower)) {
      continue;
    }

    const auto required_family =
        tables::kArchitectureDetectMetalFamilies[index];
    if (!required_family.empty() &&
        toLowerCopy(required_family) != metal_lower) {
      continue;
    }

    return arch;
  }

  return fallback;
}

/**
 * @copydoc orteaf::internal::architecture::detectMpsArchitectureForDeviceId
 */
Architecture detectMpsArchitectureForDeviceId(
    ::orteaf::internal::base::DeviceHandle device_id) {
#if ORTEAF_ENABLE_MPS
  const std::uint32_t device_index = static_cast<std::uint32_t>(device_id);
  const auto backend_unavailable = diagnostics::error::makeErrorCode(
      diagnostics::error::OrteafErrc::BackendUnavailable);

  try {
    int count =
        ::orteaf::internal::execution::mps::platform::wrapper::getDeviceCount();
    if (count <= 0 || device_index >= static_cast<std::uint32_t>(count)) {
      return Architecture::MpsGeneric;
    }

    ::orteaf::internal::execution::mps::platform::wrapper::MpsDevice_t device =
        ::orteaf::internal::execution::mps::platform::wrapper::getDevice(
            static_cast<
                ::orteaf::internal::execution::mps::platform::wrapper::MPSInt_t>(
                device_index));
    if (device == nullptr) {
      return Architecture::MpsGeneric;
    }

    ScopedDevice guard(device);
    std::string metal_family = ::orteaf::internal::execution::mps::platform::
        wrapper::getDeviceMetalFamily(device);
    std::string vendor =
        ::orteaf::internal::execution::mps::platform::wrapper::getDeviceVendor(
            device);
    if (vendor.empty()) {
      vendor = "apple";
    }
    return detectMpsArchitecture(metal_family, vendor);
  } catch (const std::system_error &err) {
    if (err.code() == backend_unavailable) {
      return Architecture::MpsGeneric;
    }
    throw;
  }
#else
  (void)device_id;
  return Architecture::MpsGeneric;
#endif
}

} // namespace orteaf::internal::architecture

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace orteaf::internal::backend {

/// @brief Enumerates the available backends (CPU / CUDA / MPS).
///
/// The ordering matches `configs/backend/backends.yml`, so these values can be used
/// as direct indices into the generated metadata tables.
enum class Backend : std::uint16_t {
#define BACKEND(ID, DISPLAY_NAME, MODULE_PATH, DESCRIPTION) ID,
#include <orteaf/backend/backend.def>
#undef BACKEND
    Count,
};

/// @brief Convert a backend to its generated-table index.
constexpr std::size_t toIndex(Backend backend) {
    return static_cast<std::size_t>(backend);
}

inline constexpr std::size_t kBackendCount = static_cast<std::size_t>(Backend::Count);

}  // namespace orteaf::internal::backend

#include <orteaf/backend/backend_tables.h>

namespace orteaf::internal::backend {

namespace tables = ::orteaf::generated::backend_tables;

static_assert(kBackendCount == tables::kBackendCount,
              "Backend enum size must match generated table size");

inline constexpr std::array<std::string_view, kBackendCount> kBackendIds = {
#define BACKEND(ID, DISPLAY_NAME, MODULE_PATH, DESCRIPTION) std::string_view{#ID},
#include <orteaf/backend/backend.def>
#undef BACKEND
};

inline constexpr std::array<Backend, kBackendCount> kAllBackends = {
#define BACKEND(ID, DISPLAY_NAME, MODULE_PATH, DESCRIPTION) Backend::ID,
#include <orteaf/backend/backend.def>
#undef BACKEND
};

/// @brief Return whether an index is within range.
constexpr bool isValidIndex(std::size_t index) {
    return index < kBackendCount;
}

/// @brief Convert an index back into the enum value.
constexpr Backend fromIndex(std::size_t index) {
    return static_cast<Backend>(index);
}

/// @brief Return the YAML identifier (e.g. `"cuda"`).
constexpr std::string_view idOf(Backend backend) {
    return kBackendIds[toIndex(backend)];
}

/// @brief Return the display name.
inline constexpr std::string_view displayNameOf(Backend backend) {
    return tables::kBackendDisplayNames[toIndex(backend)];
}

inline constexpr std::string_view modulePathOf(Backend backend) {
    return tables::kBackendModulePaths[toIndex(backend)];
}

inline constexpr std::string_view descriptionOf(Backend backend) {
    return tables::kBackendDescriptions[toIndex(backend)];
}

inline constexpr std::span<const Backend> allBackends() {
    return std::span<const Backend>(kAllBackends.data(), kAllBackends.size());
}

}  // namespace orteaf::internal::backend

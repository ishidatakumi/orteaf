#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <orteaf/internal/backend/backend.h>

namespace orteaf::internal::architecture {

/// @brief Enumerates per-backend optimization architectures (Generic, Sm90, etc.).
///
/// The generator serializes architectures as `(backend, local_index)` pairs. Local
/// index `0` is automatically reserved for the Generic entry and can be used as a
/// universal fallback.
enum class Architecture : std::uint16_t {
#define ARCHITECTURE(ENUM_NAME, BACKEND_ENUM, LOCAL_INDEX, ID, DISPLAY_NAME, DESCRIPTION) ENUM_NAME,
#include <orteaf/architecture/architecture.def>
#undef ARCHITECTURE
    Count,
};

/// @brief Convert an architecture to its generated-table index.
constexpr std::size_t toIndex(Architecture arch) {
    return static_cast<std::size_t>(arch);
}

inline constexpr std::size_t kArchitectureCount = static_cast<std::size_t>(Architecture::Count);

}  // namespace orteaf::internal::architecture

#include <orteaf/architecture/architecture_tables.h>

namespace orteaf::internal::architecture {

namespace tables = ::orteaf::generated::architecture_tables;

static_assert(kArchitectureCount == tables::kArchitectureCount,
              "Architecture enum size must match generated table size");
static_assert(backend::kBackendCount == tables::kBackendCount,
              "Architecture metadata must match backend count");

inline constexpr std::array<Architecture, kArchitectureCount> kAllArchitectures = {
#define ARCHITECTURE(ENUM_NAME, BACKEND_ENUM, LOCAL_INDEX, ID, DISPLAY_NAME, DESCRIPTION) Architecture::ENUM_NAME,
#include <orteaf/architecture/architecture.def>
#undef ARCHITECTURE
};

inline constexpr std::array<std::string_view, kArchitectureCount> kArchitectureIds = {
#define ARCHITECTURE(ENUM_NAME, BACKEND_ENUM, LOCAL_INDEX, ID, DISPLAY_NAME, DESCRIPTION) std::string_view{ID},
#include <orteaf/architecture/architecture.def>
#undef ARCHITECTURE
};

inline constexpr std::array<std::string_view, kArchitectureCount> kArchitectureDisplayNames = {
#define ARCHITECTURE(ENUM_NAME, BACKEND_ENUM, LOCAL_INDEX, ID, DISPLAY_NAME, DESCRIPTION) std::string_view{DISPLAY_NAME},
#include <orteaf/architecture/architecture.def>
#undef ARCHITECTURE
};

/// @brief Check whether an index is within range.
constexpr bool isValidIndex(std::size_t index) {
    return index < kArchitectureCount;
}

/// @brief Convert an index back into the enum value.
constexpr Architecture fromIndex(std::size_t index) {
    return static_cast<Architecture>(index);
}

/// @brief Return the backend the architecture belongs to.
constexpr backend::Backend backendOf(Architecture arch) {
    const auto backend_index = tables::kArchitectureBackendIndices[toIndex(arch)];
    return backend::fromIndex(backend_index);
}

/// @brief Return the backend-local index (0 == Generic).
constexpr std::uint16_t localIndexOf(Architecture arch) {
    return tables::kArchitectureLocalIndices[toIndex(arch)];
}

/// @brief Return true if the architecture is the Generic entry.
constexpr bool isGeneric(Architecture arch) {
    return localIndexOf(arch) == 0;
}

/// @brief Return the YAML identifier / display name / description.
constexpr std::string_view idOf(Architecture arch) {
    return tables::kArchitectureIds[toIndex(arch)];
}

constexpr std::string_view displayNameOf(Architecture arch) {
    return tables::kArchitectureDisplayNames[toIndex(arch)];
}

constexpr std::string_view descriptionOf(Architecture arch) {
    return tables::kArchitectureDescriptions[toIndex(arch)];
}

constexpr std::size_t countForBackend(backend::Backend backend_id) {
    return tables::kBackendArchitectureCounts[backend::toIndex(backend_id)];
}

constexpr std::size_t offsetForBackend(backend::Backend backend_id) {
    return tables::kBackendArchitectureOffsets[backend::toIndex(backend_id)];
}

constexpr bool hasLocalIndex(backend::Backend backend_id, std::uint16_t local_index) {
    return local_index < countForBackend(backend_id);
}

/// @brief Construct an architecture from a backend and local index.
constexpr Architecture fromBackendAndLocalIndex(backend::Backend backend_id, std::uint16_t local_index) {
    return static_cast<Architecture>(offsetForBackend(backend_id) + local_index);
}

/// @brief Return a span of every architecture belonging to the backend.
inline constexpr std::span<const Architecture> architecturesOf(backend::Backend backend_id) {
    const auto offset = offsetForBackend(backend_id);
    const auto count = countForBackend(backend_id);
    return std::span<const Architecture>(kAllArchitectures.data() + offset, count);
}

}  // namespace orteaf::internal::architecture

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <orteaf/internal/backend/backend.h>

namespace orteaf::internal::architecture {

enum class Architecture : std::uint16_t {
#define ARCHITECTURE(ENUM_NAME, BACKEND_ENUM, LOCAL_INDEX, ID, DISPLAY_NAME, DESCRIPTION) ENUM_NAME,
#include <orteaf/architecture/architecture.def>
#undef ARCHITECTURE
    Count,
};

constexpr std::size_t ToIndex(Architecture arch) {
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

constexpr bool IsValidIndex(std::size_t index) {
    return index < kArchitectureCount;
}

constexpr Architecture FromIndex(std::size_t index) {
    return static_cast<Architecture>(index);
}

constexpr backend::Backend BackendOf(Architecture arch) {
    const auto backend_index = tables::kArchitectureBackendIndices[ToIndex(arch)];
    return backend::FromIndex(backend_index);
}

constexpr std::uint16_t LocalIndexOf(Architecture arch) {
    return tables::kArchitectureLocalIndices[ToIndex(arch)];
}

constexpr bool IsGeneric(Architecture arch) {
    return LocalIndexOf(arch) == 0;
}

constexpr std::string_view IdOf(Architecture arch) {
    return tables::kArchitectureIds[ToIndex(arch)];
}

constexpr std::string_view DisplayNameOf(Architecture arch) {
    return tables::kArchitectureDisplayNames[ToIndex(arch)];
}

constexpr std::string_view DescriptionOf(Architecture arch) {
    return tables::kArchitectureDescriptions[ToIndex(arch)];
}

constexpr std::size_t CountForBackend(backend::Backend backend_id) {
    return tables::kBackendArchitectureCounts[backend::ToIndex(backend_id)];
}

constexpr std::size_t OffsetForBackend(backend::Backend backend_id) {
    return tables::kBackendArchitectureOffsets[backend::ToIndex(backend_id)];
}

constexpr bool HasLocalIndex(backend::Backend backend_id, std::uint16_t local_index) {
    return local_index < CountForBackend(backend_id);
}

constexpr Architecture FromBackendAndLocalIndex(backend::Backend backend_id, std::uint16_t local_index) {
    return static_cast<Architecture>(OffsetForBackend(backend_id) + local_index);
}

inline constexpr std::span<const Architecture> ArchitecturesOf(backend::Backend backend_id) {
    const auto offset = OffsetForBackend(backend_id);
    const auto count = CountForBackend(backend_id);
    return std::span<const Architecture>(kAllArchitectures.data() + offset, count);
}

}  // namespace orteaf::internal::architecture

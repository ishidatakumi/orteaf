#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "float8.h"
#include "float16.h"

namespace orteaf::internal {

/// @brief Enumerates every dtype declared in `configs/dtype/dtypes.yml`.
///
/// The enum order exactly matches `dtype.def`, allowing the values to act as direct
/// indices into the generated metadata tables. The trailing `Count` sentinel reports
/// the total number of entries at compile time.
enum class DType : std::uint16_t {
#define DTYPE(ID, CPP_TYPE, DISPLAY_NAME) ID,
#include <orteaf/dtype/dtype.def>
#undef DTYPE
    Count,
};

/// @brief Convert a dtype to its stable index within the generated tables.
constexpr std::size_t toIndex(DType dtype) {
    return static_cast<std::size_t>(dtype);
}

inline constexpr std::size_t kDTypeCount = static_cast<std::size_t>(DType::Count);

}  // namespace orteaf::internal

#include <orteaf/dtype/dtype_tables.h>

namespace orteaf::internal {

namespace generated_tables = ::orteaf::generated::dtype_tables;

static_assert(kDTypeCount == generated_tables::kDTypeCount,
              "DType enum size must match generated table size");

inline constexpr std::array<std::string_view, kDTypeCount> kDTypeIds = {
#define DTYPE(ID, CPP_TYPE, DISPLAY_NAME) std::string_view{#ID},
#include <orteaf/dtype/dtype.def>
#undef DTYPE
};

inline constexpr std::array<DType, kDTypeCount> kAllDTypes = {
#define DTYPE(ID, CPP_TYPE, DISPLAY_NAME) DType::ID,
#include <orteaf/dtype/dtype.def>
#undef DTYPE
};

enum class CastMode {
    Implicit,
    Explicit,
};

/// @brief Check whether an index is within the valid dtype range.
constexpr bool isValidIndex(std::size_t index) {
    return index < kDTypeCount;
}

/// @brief Convert an index back into the corresponding enum value.
constexpr DType fromIndex(std::size_t index) {
    return static_cast<DType>(index);
}

/// @brief Return the YAML identifier (e.g. `"F32"`).
constexpr std::string_view idOf(DType dtype) {
    return kDTypeIds[toIndex(dtype)];
}

/// @brief Return the human-readable display name (e.g. `"float32"`).
constexpr std::string_view displayNameOf(DType dtype) {
    return generated_tables::kDTypeDisplayNames[toIndex(dtype)];
}

/// @brief Return the category string this dtype belongs to.
constexpr std::string_view categoryOf(DType dtype) {
    return generated_tables::kDTypeCategories[toIndex(dtype)];
}

/// @brief Return the promotion priority (higher means more precise).
constexpr int promotionPriority(DType dtype) {
    return generated_tables::kDTypePromotionPriorities[toIndex(dtype)];
}

/// @brief Return the element size in bytes.
constexpr std::size_t sizeOf(DType dtype) {
    return generated_tables::kDTypeSize[toIndex(dtype)];
}

/// @brief Return the required alignment in bytes.
constexpr std::size_t alignmentOf(DType dtype) {
    return generated_tables::kDTypeAlignment[toIndex(dtype)];
}

/// @brief Return the compute dtype (e.g. FP8 promotes to FP16 when accumulated).
constexpr DType computeType(DType dtype) {
    return generated_tables::kDTypeComputeType[toIndex(dtype)];
}

/// @brief Return the promotion result of two dtypes.
constexpr DType promote(DType lhs, DType rhs) {
    return generated_tables::kPromotionTable[toIndex(lhs)][toIndex(rhs)];
}

/// @brief Return whether casting is allowed under the specified mode.
constexpr bool canCast(DType from, DType to, CastMode mode) {
    const auto from_index = toIndex(from);
    const auto to_index = toIndex(to);
    const auto& matrix = (mode == CastMode::Implicit)
                             ? generated_tables::kImplicitCastMatrix[from_index]
                             : generated_tables::kExplicitCastMatrix[from_index];
    return matrix[to_index];
}

/// @brief Convenience helper for implicit casts.
constexpr bool canImplicitlyCast(DType from, DType to) {
    return canCast(from, to, CastMode::Implicit);
}

/// @brief Convenience helper for explicit casts.
constexpr bool canExplicitlyCast(DType from, DType to) {
    return canCast(from, to, CastMode::Explicit);
}

}  // namespace orteaf::internal

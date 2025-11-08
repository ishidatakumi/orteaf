#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace orteaf::internal {

enum class DType : std::uint16_t {
#define DTYPE(ID, CPP_TYPE, DISPLAY_NAME) ID,
#include <orteaf/dtype/dtype.def>
#undef DTYPE
    Count,
};

constexpr std::size_t ToIndex(DType dtype) {
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

constexpr bool IsValidIndex(std::size_t index) {
    return index < kDTypeCount;
}

constexpr DType FromIndex(std::size_t index) {
    return static_cast<DType>(index);
}

constexpr std::string_view IdOf(DType dtype) {
    return kDTypeIds[ToIndex(dtype)];
}

constexpr std::string_view DisplayNameOf(DType dtype) {
    return generated_tables::kDTypeDisplayNames[ToIndex(dtype)];
}

constexpr std::string_view CategoryOf(DType dtype) {
    return generated_tables::kDTypeCategories[ToIndex(dtype)];
}

constexpr int PromotionPriority(DType dtype) {
    return generated_tables::kDTypePromotionPriorities[ToIndex(dtype)];
}

constexpr std::size_t SizeOf(DType dtype) {
    return generated_tables::kDTypeSize[ToIndex(dtype)];
}

constexpr std::size_t AlignmentOf(DType dtype) {
    return generated_tables::kDTypeAlignment[ToIndex(dtype)];
}

constexpr DType ComputeType(DType dtype) {
    return generated_tables::kDTypeComputeType[ToIndex(dtype)];
}

constexpr DType Promote(DType lhs, DType rhs) {
    return generated_tables::kPromotionTable[ToIndex(lhs)][ToIndex(rhs)];
}

constexpr bool CanCast(DType from, DType to, CastMode mode) {
    const auto from_index = ToIndex(from);
    const auto to_index = ToIndex(to);
    const auto& matrix = (mode == CastMode::Implicit)
                             ? generated_tables::kImplicitCastMatrix[from_index]
                             : generated_tables::kExplicitCastMatrix[from_index];
    return matrix.test(to_index);
}

constexpr bool CanImplicitlyCast(DType from, DType to) {
    return CanCast(from, to, CastMode::Implicit);
}

constexpr bool CanExplicitlyCast(DType from, DType to) {
    return CanCast(from, to, CastMode::Explicit);
}

}  // namespace orteaf::internal

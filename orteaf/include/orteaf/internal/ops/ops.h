#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <orteaf/internal/dtype/dtype.h>

namespace orteaf::internal::ops {

/// @brief Enumerates operations defined in `configs/ops/ops.yml`.
///
/// The ordering matches `ops.def`, making the enum values stable indices into the
/// generated metadata tables.
enum class Op : std::uint16_t {
#define ORTEAF_OP(ID, DISPLAY_NAME, CATEGORY) ID,
#include <orteaf/ops/ops.def>
#undef ORTEAF_OP
    Count,
};

/// @brief Convert an op to its generated-table index.
constexpr std::size_t toIndex(Op op) {
    return static_cast<std::size_t>(op);
}

inline constexpr std::size_t kOpCount = static_cast<std::size_t>(Op::Count);

}  // namespace orteaf::internal::ops

#include <orteaf/ops/ops_tables.h>

namespace orteaf::internal::ops {

namespace tables = ::orteaf::generated::ops_tables;

static_assert(kOpCount == tables::kOpCount, "Op enum size must match generated table size");

inline constexpr std::array<std::string_view, kOpCount> kOpIds = {
#define ORTEAF_OP(ID, DISPLAY_NAME, CATEGORY) std::string_view{#ID},
#include <orteaf/ops/ops.def>
#undef ORTEAF_OP
};

inline constexpr std::array<Op, kOpCount> kAllOps = {
#define ORTEAF_OP(ID, DISPLAY_NAME, CATEGORY) Op::ID,
#include <orteaf/ops/ops.def>
#undef ORTEAF_OP
};

using DTypeConstraintMode = tables::DTypeConstraintMode;
using DTypeRuleKind = tables::DTypeRuleKind;
using ComputePolicyKind = tables::ComputePolicyKind;
using InputSpec = tables::InputSpec;
using OutputSpec = tables::OutputSpec;
using AttributeSpec = tables::AttributeSpec;
using ComputePolicySpec = tables::ComputePolicySpec;
using ShapeInferenceSpec = tables::ShapeInferenceSpec;
using MetadataSpec = tables::MetadataSpec;

constexpr bool isValidIndex(std::size_t index) {
    return index < kOpCount;
}

/// @brief Convert an index back into the enum value.
constexpr Op fromIndex(std::size_t index) {
    return static_cast<Op>(index);
}

/// @brief Return the YAML identifier (e.g. `"MatMul"`).
constexpr std::string_view idOf(Op op) {
    return kOpIds[toIndex(op)];
}

/// @brief Return the display name.
inline constexpr std::string_view displayNameOf(Op op) {
    return tables::kOpDisplayNames[toIndex(op)];
}

/// @brief Return the category string.
inline constexpr std::string_view categoryOf(Op op) {
    return tables::kOpCategories[toIndex(op)];
}

/// @brief Return the arity (number of inputs).
inline constexpr std::uint32_t arityOf(Op op) {
    return tables::kOpArity[toIndex(op)];
}

/// @brief Return the compute-policy specification.
inline constexpr const ComputePolicySpec& computePolicyOf(Op op) {
    return tables::kOpComputePolicies[toIndex(op)];
}

/// @brief Return the shape-inference specification.
inline constexpr const ShapeInferenceSpec& shapeInferenceOf(Op op) {
    return tables::kOpShapeInference[toIndex(op)];
}

/// @brief Return the metadata block (tags, aliases, etc.).
inline constexpr const MetadataSpec& metadataOf(Op op) {
    return tables::kOpMetadata[toIndex(op)];
}

template <typename T, std::size_t N>
constexpr std::span<const T> slice(const std::array<T, N>& data, tables::Range range) {
    return std::span<const T>(data.data() + range.offset, range.count);
}

inline constexpr std::span<const InputSpec> inputsOf(Op op) {
    return slice(tables::kOpInputSpecs, tables::kOpInputRanges[toIndex(op)]);
}

/// @brief Return the list of output specs.
inline constexpr std::span<const OutputSpec> outputsOf(Op op) {
    return slice(tables::kOpOutputSpecs, tables::kOpOutputRanges[toIndex(op)]);
}

/// @brief Return the list of attribute specs.
inline constexpr std::span<const AttributeSpec> attributesOf(Op op) {
    return slice(tables::kOpAttributes, tables::kOpAttributeRanges[toIndex(op)]);
}

/// @brief Return the metadata tags.
inline constexpr std::span<const std::string_view> tagsOf(Op op) {
    return slice(tables::kOpMetadataTags, metadataOf(op).tags);
}

/// @brief Return the metadata aliases.
inline constexpr std::span<const std::string_view> aliasesOf(Op op) {
    return slice(tables::kOpMetadataAliases, metadataOf(op).aliases);
}

/// @brief Enumerate every op.
inline constexpr std::span<const Op> allOps() {
    return std::span<const Op>(kAllOps.data(), kAllOps.size());
}

/// @brief Enumerate every op identifier.
inline constexpr std::span<const std::string_view> allOpIds() {
    return std::span<const std::string_view>(kOpIds.data(), kOpIds.size());
}

}  // namespace orteaf::internal::ops

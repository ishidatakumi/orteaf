#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <string_view>

#include <orteaf/internal/ops/ops.h>

namespace ops = orteaf::internal::ops;
namespace dtype = orteaf::internal;

namespace {

std::size_t dTypeBit(dtype::DType dt) {
    return static_cast<std::size_t>(dtype::toIndex(dt));
}

bool maskIncludes(const ops::InputSpec& spec, dtype::DType dt) {
    return (spec.dtype.allow_mask & (1ULL << dTypeBit(dt))) != 0ULL;
}

bool maskDenies(const ops::InputSpec& spec, dtype::DType dt) {
    return (spec.dtype.deny_mask & (1ULL << dTypeBit(dt))) != 0ULL;
}

template <typename SpanT>
bool containsString(SpanT span, std::string_view target) {
    return std::find(span.begin(), span.end(), target) != span.end();
}

}  // namespace

TEST(OpsTablesTest, BasicEnumerationProperties) {
    EXPECT_EQ(ops::kOpCount, ops::allOps().size());
    EXPECT_TRUE(ops::isValidIndex(0));
    EXPECT_FALSE(ops::isValidIndex(ops::kOpCount));

    EXPECT_EQ(ops::fromIndex(0), ops::allOps().front());
    EXPECT_EQ(ops::idOf(ops::fromIndex(0)), ops::allOpIds().front());
}

TEST(OpsTablesTest, AddOpMetadata) {
    constexpr auto op = ops::Op::Add;
    EXPECT_EQ(ops::displayNameOf(op), "Add");
    EXPECT_EQ(ops::categoryOf(op), "arithmetic");
    EXPECT_EQ(ops::arityOf(op), 2U);

    const auto inputs = ops::inputsOf(op);
    ASSERT_EQ(inputs.size(), 2U);

    const auto& lhs = inputs[0];
    EXPECT_EQ(lhs.dtype.mode, ops::DTypeConstraintMode::Allow);
    EXPECT_TRUE(maskIncludes(lhs, dtype::DType::F32));
    EXPECT_TRUE(maskIncludes(lhs, dtype::DType::Bool));
    EXPECT_FALSE(lhs.dtype.allow_promotion);
    EXPECT_FALSE(lhs.dtype.require_same_shape);

    const auto& rhs = inputs[1];
    EXPECT_EQ(rhs.dtype.mode, ops::DTypeConstraintMode::Match);
    EXPECT_EQ(rhs.dtype.reference_input, 0U);
    EXPECT_TRUE(rhs.dtype.allow_promotion);
    EXPECT_FALSE(rhs.dtype.require_same_shape);

    const auto outputs = ops::outputsOf(op);
    ASSERT_EQ(outputs.size(), 1U);
    EXPECT_EQ(outputs[0].kind, ops::DTypeRuleKind::Promote);

    const auto& compute_policy = ops::computePolicyOf(op);
    EXPECT_EQ(compute_policy.kind, ops::ComputePolicyKind::Promote);
    EXPECT_NE(compute_policy.input_mask, 0ULL);

    const auto tags = ops::tagsOf(op);
    EXPECT_TRUE(containsString(tags, "elementwise"));
    EXPECT_TRUE(ops::metadataOf(op).commutative);

    const auto aliases = ops::aliasesOf(op);
    EXPECT_TRUE(containsString(aliases, "add"));
}

TEST(OpsTablesTest, MatMulOptionalInputsAndAttributes) {
    constexpr auto op = ops::Op::MatMul;

    const auto inputs = ops::inputsOf(op);
    ASSERT_EQ(inputs.size(), 3U);
    EXPECT_FALSE(inputs[0].optional);
    EXPECT_TRUE(inputs[2].optional);
    EXPECT_EQ(inputs[2].dtype.mode, ops::DTypeConstraintMode::Allow);
    EXPECT_TRUE(maskIncludes(inputs[2], dtype::DType::F32));

    const auto attributes = ops::attributesOf(op);
    ASSERT_EQ(attributes.size(), 2U);
    EXPECT_EQ(attributes[0].name, "transposed_lhs");
    EXPECT_EQ(attributes[0].type, "bool");
    EXPECT_EQ(attributes[0].default_value, std::optional<std::string>{"false"});

    const auto& shape = ops::shapeInferenceOf(op);
    EXPECT_EQ(shape.kind, "matmul");
    EXPECT_TRUE(shape.function.empty());  // matmul kind doesn't require a function

    const auto& compute_policy = ops::computePolicyOf(op);
    EXPECT_EQ(compute_policy.kind, ops::ComputePolicyKind::DerivedComputeType);
    EXPECT_EQ(compute_policy.function, "MatMulAccumulatorType");
}

TEST(OpsTablesTest, SpikeThresholdDeniesBooleanThreshold) {
    constexpr auto op = ops::Op::SpikeThreshold;
    const auto inputs = ops::inputsOf(op);
    ASSERT_EQ(inputs.size(), 2U);
    const auto& threshold = inputs[1];
    EXPECT_EQ(threshold.dtype.mode, ops::DTypeConstraintMode::Deny);
    EXPECT_TRUE(maskDenies(threshold, dtype::DType::Bool));

    const auto outputs = ops::outputsOf(op);
    ASSERT_EQ(outputs.size(), 1U);
    EXPECT_EQ(outputs[0].kind, ops::DTypeRuleKind::Fixed);
    EXPECT_EQ(outputs[0].fixed_dtype, dtype::toIndex(dtype::DType::Bool));

    const auto& compute_policy = ops::computePolicyOf(op);
    EXPECT_EQ(compute_policy.kind, ops::ComputePolicyKind::Fixed);
    EXPECT_EQ(compute_policy.fixed_dtype, dtype::toIndex(dtype::DType::Bool));

    const auto metadata = ops::metadataOf(op);
    EXPECT_FALSE(metadata.differentiable);
    EXPECT_TRUE(containsString(ops::tagsOf(op), "spiking"));
}

TEST(OpsTablesTest, CustomQuantizeUsesCustomHooks) {
    constexpr auto op = ops::Op::CustomQuantize;
    const auto outputs = ops::outputsOf(op);
    ASSERT_EQ(outputs.size(), 1U);
    EXPECT_EQ(outputs[0].kind, ops::DTypeRuleKind::Custom);
    EXPECT_EQ(outputs[0].custom_function, "InferQuantizedDType");

    const auto& compute_policy = ops::computePolicyOf(op);
    EXPECT_EQ(compute_policy.kind, ops::ComputePolicyKind::Custom);
    EXPECT_EQ(compute_policy.handler, "SelectQuantizedComputeType");

    const auto attributes = ops::attributesOf(op);
    ASSERT_EQ(attributes.size(), 2U);
    EXPECT_EQ(attributes[0].type, "int");
    EXPECT_EQ(attributes[0].default_value, std::optional<std::string>{"8"});
    EXPECT_EQ(attributes[1].type, "bool");
    EXPECT_EQ(attributes[1].default_value, std::optional<std::string>{"true"});
}

TEST(OpsTablesTest, ReluMetadataAndShape) {
    constexpr auto op = ops::Op::Relu;
    EXPECT_EQ(ops::arityOf(op), 1U);
    EXPECT_EQ(ops::categoryOf(op), "activation");

    const auto& metadata = ops::metadataOf(op);
    EXPECT_TRUE(metadata.differentiable);
    EXPECT_TRUE(containsString(ops::tagsOf(op), "activation"));

    const auto& shape = ops::shapeInferenceOf(op);
    EXPECT_EQ(shape.kind, "identity");
    EXPECT_TRUE(shape.function.empty());
}


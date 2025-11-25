#include "orteaf/internal/runtime/allocator/policies/chunk_locator/chunk_locator_concept.h"
#include "orteaf/internal/runtime/allocator/policies/chunk_locator/direct_chunk_locator.h"
#include "orteaf/internal/runtime/allocator/policies/chunk_locator/hierarchical_chunk_locator.h"

#include <gtest/gtest.h>

#include "orteaf/internal/backend/backend.h"
#include "tests/internal/runtime/allocator/testing/mock_resource.h"

namespace policies = ::orteaf::internal::runtime::allocator::policies;
using Backend = ::orteaf::internal::backend::Backend;
using ::orteaf::internal::runtime::allocator::testing::MockCpuResource;

namespace {

// Direct ポリシーの型定義
using DirectPolicy = policies::DirectChunkLocatorPolicy<MockCpuResource, Backend::Cpu>;
using DirectConfig = DirectPolicy::Config;

// Hierarchical ポリシーの型定義
using HierarchicalPolicy = policies::HierarchicalChunkLocator<MockCpuResource, Backend::Cpu>;
using HierarchicalConfig = HierarchicalPolicy::Config;

// ============================================================================
// コンパイル時検証: static_assert で concept を満たすことを確認
// ============================================================================

// DirectChunkLocatorPolicy が ChunkLocator concept を満たす
static_assert(
    policies::ChunkLocator<DirectPolicy, DirectConfig, MockCpuResource>,
    "DirectChunkLocatorPolicy must satisfy ChunkLocator concept"
);

// HierarchicalChunkLocator が ChunkLocator concept を満たす
static_assert(
    policies::ChunkLocator<HierarchicalPolicy, HierarchicalConfig, MockCpuResource>,
    "HierarchicalChunkLocator must satisfy ChunkLocator concept"
);

// 両方が標準の BufferId を使用している
static_assert(
    policies::HasStandardBufferId<DirectPolicy>,
    "DirectChunkLocatorPolicy must use standard BufferId"
);

static_assert(
    policies::HasStandardBufferId<HierarchicalPolicy>,
    "HierarchicalChunkLocator must use standard BufferId"
);

// Config が共通の基底フィールドを持つ
static_assert(
    policies::ChunkLocatorConfigDerived<
        DirectConfig,
        DirectPolicy::Device,
        DirectPolicy::Context,
        DirectPolicy::Stream
    >,
    "DirectConfig must have base config fields"
);

static_assert(
    policies::ChunkLocatorConfigDerived<
        HierarchicalConfig,
        HierarchicalPolicy::Device,
        HierarchicalPolicy::Context,
        HierarchicalPolicy::Stream
    >,
    "HierarchicalConfig must have base config fields"
);

// ============================================================================
// ランタイムテスト: concept を満たす型を使ったジェネリック関数のテスト
// ============================================================================

// concept を使ったジェネリック関数の例
template <typename Policy, typename Config, typename Resource>
    requires policies::ChunkLocator<Policy, Config, Resource>
bool testChunkLocatorInterface(Policy& policy, const Config& config, Resource* resource) {
    // initialize が呼べる
    policy.initialize(config, resource);
    return true;
}

TEST(ChunkLocatorConcept, DirectPolicySatisfiesConcept) {
    DirectPolicy policy;
    DirectConfig config{};
    MockCpuResource resource;
    
    // コンパイルが通れば concept を満たしている
    EXPECT_TRUE((testChunkLocatorInterface<DirectPolicy, DirectConfig, MockCpuResource>(
        policy, config, &resource)));
}

TEST(ChunkLocatorConcept, HierarchicalPolicySatisfiesConcept) {
    HierarchicalPolicy policy;
    HierarchicalConfig config{};
    config.levels = {256, 128};
    MockCpuResource resource;
    
    // コンパイルが通れば concept を満たしている
    EXPECT_TRUE((testChunkLocatorInterface<HierarchicalPolicy, HierarchicalConfig, MockCpuResource>(
        policy, config, &resource)));
}

}  // namespace

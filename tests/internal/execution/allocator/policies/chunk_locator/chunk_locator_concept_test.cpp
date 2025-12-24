#include "orteaf/internal/execution/allocator/policies/chunk_locator/chunk_locator_concept.h"
#include "orteaf/internal/execution/allocator/policies/chunk_locator/direct_chunk_locator.h"

#include <gtest/gtest.h>

#include "orteaf/internal/backend/backend.h"
#include "tests/internal/execution/allocator/testing/mock_resource.h"

namespace policies = ::orteaf::internal::execution::allocator::policies;
using Backend = ::orteaf::internal::backend::Backend;
using ::orteaf::internal::execution::allocator::testing::MockCpuResource;

namespace {

// Direct ポリシーの型定義
using DirectPolicy = policies::DirectChunkLocatorPolicy<MockCpuResource>;
using DirectConfig = DirectPolicy::Config;

// ============================================================================
// コンパイル時検証: static_assert で concept を満たすことを確認
// ============================================================================

// DirectChunkLocatorPolicy が ChunkLocator concept を満たす
static_assert(
    policies::ChunkLocator<DirectPolicy, DirectConfig, MockCpuResource>,
    "DirectChunkLocatorPolicy must satisfy ChunkLocator concept");

// 標準の BufferViewHandle を使用している
static_assert(policies::HasStandardBufferViewHandle<DirectPolicy>,
              "DirectChunkLocatorPolicy must use standard BufferViewHandle");

// ============================================================================
// ランタイムテスト: concept を満たす型を使ったジェネリック関数のテスト
// ============================================================================

// concept を使ったジェネリック関数の例
template <typename Policy, typename Config, typename Resource>
  requires policies::ChunkLocator<Policy, Config, Resource>
bool testChunkLocatorInterface(Policy &policy, const Config &config,
                               Resource *resource) {
  // initialize が呼べる
  policy.initialize(config);
  return true;
}

TEST(ChunkLocatorConcept, DirectPolicySatisfiesConcept) {
  DirectPolicy policy;
  MockCpuResource resource;
  DirectConfig config{};
  config.resource = &resource;

  // コンパイルが通れば concept を満たしている
  EXPECT_TRUE(
      (testChunkLocatorInterface<DirectPolicy, DirectConfig, MockCpuResource>(
          policy, config, &resource)));
}

} // namespace

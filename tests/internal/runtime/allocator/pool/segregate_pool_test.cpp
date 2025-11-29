#include "orteaf/internal/runtime/allocator/pool/segregate_pool.h"

#include <gtest/gtest.h>

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/runtime/allocator/memory_block.h"
#include "orteaf/internal/runtime/allocator/policies/policy_config.h"

namespace {

using ::orteaf::internal::backend::Backend;
using ::orteaf::internal::runtime::allocator::MemoryBlock;
namespace policies = ::orteaf::internal::runtime::allocator::policies;

struct DummyResource {
    using MemoryBlock = ::orteaf::internal::runtime::allocator::MemoryBlock<Backend::Cpu>;
};

// Template config style policy (FastFree/Threading 互換)
struct TemplateInitPolicy {
    template <typename Resource>
    struct Config : policies::PolicyConfig<Resource> {};

    template <typename Resource>
    void initialize(const Config<Resource>& cfg) {
        initialized = (cfg.resource != nullptr);
    }

    bool initialized{false};
};

// Non-template config style policy (LargeAlloc/ChunkLocator/Reuse/FreeList 互換)
struct InitPolicy {
    struct Config : policies::PolicyConfig<DummyResource> {};

    void initialize(const Config& cfg) { initialized = (cfg.resource != nullptr); }

    bool initialized{false};
};

using Pool = ::orteaf::internal::runtime::allocator::pool::SegregatePool<
    DummyResource,
    TemplateInitPolicy,
    TemplateInitPolicy,
    InitPolicy,
    InitPolicy,
    InitPolicy,
    InitPolicy,
    Backend::Cpu>;

TEST(SegregatePool, InitializePropagatesToAllPolicies) {
    Pool pool;
    Pool::Config cfg{};
    DummyResource resource{};

    cfg.fast_free.resource = &resource;
    cfg.threading.resource = &resource;
    cfg.large_alloc.resource = &resource;
    cfg.chunk_locator.resource = &resource;
    cfg.reuse.resource = &resource;
    cfg.freelist.resource = &resource;

    pool.initialize(cfg);

    EXPECT_TRUE(pool.fast_free_policy().initialized);
    EXPECT_TRUE(pool.threading_policy().initialized);
    EXPECT_TRUE(pool.large_alloc_policy().initialized);
    EXPECT_TRUE(pool.chunk_locator_policy().initialized);
    EXPECT_TRUE(pool.reuse_policy().initialized);
    EXPECT_TRUE(pool.free_list_policy().initialized);
}

}  // namespace

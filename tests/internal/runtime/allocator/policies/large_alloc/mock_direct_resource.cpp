#include "orteaf/internal/runtime/allocator/policies/large_alloc/direct_resource_large_alloc.h"
#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/diagnostics/log/log.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>

#include "tests/internal/runtime/allocator/testing/mock_resource.h"

using ::testing::_;
using ::testing::Return;
using ::testing::HasSubstr;

namespace allocator = ::orteaf::internal::runtime::allocator;
namespace policies = ::orteaf::internal::runtime::allocator::policies;
using Backend = ::orteaf::internal::backend::Backend;
using ::orteaf::internal::runtime::allocator::testing::MockCpuResource;
using ::orteaf::internal::runtime::allocator::testing::MockCpuResourceImpl;

namespace {

#if ORTEAF_CORE_DEBUG_ENABLED
class ScopedLogCapture {
public:
    ScopedLogCapture() = delete;
    explicit ScopedLogCapture(std::string* out) : out_(out) {
        ::orteaf::internal::diagnostics::log::setLogSink(&ScopedLogCapture::sink, out_);
    }
    ScopedLogCapture(const ScopedLogCapture&) = delete;
    ScopedLogCapture& operator=(const ScopedLogCapture&) = delete;
    ScopedLogCapture(ScopedLogCapture&&) = delete;
    ScopedLogCapture& operator=(ScopedLogCapture&&) = delete;
    ~ScopedLogCapture() {
        ::orteaf::internal::diagnostics::log::resetLogSink();
    }

private:
    static void sink(::orteaf::internal::diagnostics::log::LogCategory,
                     ::orteaf::internal::diagnostics::log::LogLevel,
                     std::string_view message,
                     void* context) {
        if (context) {
            *static_cast<std::string*>(context) = std::string(message);
        }
    }

    std::string* out_{};
};
#endif  // ORTEAF_CORE_DEBUG_ENABLED

TEST(DirectResourceLargeAlloc, AllocateReturnsMemoryBlockWithId) {
    policies::DirectResourceLargeAllocPolicy<MockCpuResource, Backend::Cpu> policy;
    policy.initialize();

    ::testing::NiceMock<MockCpuResourceImpl> impl;
    MockCpuResource::set(&impl);
    EXPECT_CALL(impl, allocate(128, 64))
        .WillOnce(Return(::orteaf::internal::backend::cpu::CpuBufferView{reinterpret_cast<void*>(0x1), 0, 128}));

    auto block = policy.allocate(128, 64);
    EXPECT_TRUE(block.valid());
    EXPECT_TRUE(policy.isLargeAlloc(block.id));
    MockCpuResource::reset();
}

TEST(DirectResourceLargeAlloc, DeallocateCallsResource) {
    policies::DirectResourceLargeAllocPolicy<MockCpuResource, Backend::Cpu> policy;
    policy.initialize();

    ::orteaf::internal::backend::cpu::CpuBufferView view{reinterpret_cast<void*>(0x2), 0, 256};
    ::testing::NiceMock<MockCpuResourceImpl> impl;
    MockCpuResource::set(&impl);
    EXPECT_CALL(impl, allocate(256, 16)).WillOnce(Return(view));
    EXPECT_CALL(impl, deallocate(view, 256, 16)).Times(1);

    auto block = policy.allocate(256, 16);
    policy.deallocate(block.id, 256, 16);
    MockCpuResource::reset();
}

#if ORTEAF_CORE_DEBUG_ENABLED
TEST(DirectResourceLargeAlloc, LogsDebugWhenMetadataMismatches) {
    policies::DirectResourceLargeAllocPolicy<MockCpuResource, Backend::Cpu> policy;
    policy.initialize();

    ::testing::NiceMock<MockCpuResourceImpl> impl;
    MockCpuResource::set(&impl);
    ::orteaf::internal::backend::cpu::CpuBufferView view{reinterpret_cast<void*>(0x3), 0, 64};
    EXPECT_CALL(impl, allocate(64, 16)).WillOnce(Return(view));
    EXPECT_CALL(impl, deallocate(view, 32, 8)).Times(1);

    auto block = policy.allocate(64, 16);

    std::string captured;
    {
        ScopedLogCapture capture(&captured);
        policy.deallocate(block.id, 32, 8);
    }
    MockCpuResource::reset();

    EXPECT_FALSE(captured.empty());
    EXPECT_THAT(captured, HasSubstr("LargeAlloc deallocate mismatch"));
    EXPECT_THAT(captured, HasSubstr("recorded size=64"));
    EXPECT_THAT(captured, HasSubstr("called size=32"));
}
#endif  // ORTEAF_CORE_DEBUG_ENABLED

}  // namespace

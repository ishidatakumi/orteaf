#include <gtest/gtest.h>

#include "orteaf/internal/runtime/kernel/mps/mps_kernel_launcher_impl.h"
#include "orteaf/internal/backend/mps/mps_fast_ops.h"
#include "orteaf/internal/backend/mps/wrapper/mps_compute_command_encorder.h"
#include "orteaf/internal/runtime/ops/mps/private/mps_private_ops.h"

namespace base = orteaf::internal::base;

namespace mps_rt = orteaf::internal::runtime::mps;

TEST(MpsKernelLauncherImplTest, StoresUniqueKeysInOrder) {
    mps_rt::MpsKernelLauncherImpl<3> impl({
        {"libA", "funcX"},
        {"libB", "funcY"},
        {"libA", "funcX"},  // duplicate should be ignored
    });

    const auto& keys = impl.keysForTest();
    ASSERT_EQ(impl.sizeForTest(), 2u);

    EXPECT_EQ(keys[0].first.identifier, "libA");
    EXPECT_EQ(keys[0].second.identifier, "funcX");

    EXPECT_EQ(keys[1].first.identifier, "libB");
    EXPECT_EQ(keys[1].second.identifier, "funcY");
}

namespace {

class DummyPrivateOps {
public:
    using PipelineLease = mps_rt::MpsComputePipelineStateManager::PipelineLease;
    static void reset() {
        last_device = {};
        last_library.clear();
        last_function.clear();
    }

    // Record requests and return dummy leases.
    static PipelineLease acquirePipeline(base::DeviceHandle device,
                                         const mps_rt::LibraryKey& library_key,
                                         const mps_rt::FunctionKey& function_key) {
        last_device = device;
        last_library = library_key.identifier;
        last_function = function_key.identifier;
        // Return an empty lease; we only validate call ordering and size.
        return PipelineLease{};
    }

    static inline base::DeviceHandle last_device{};
    static inline std::string last_library{};
    static inline std::string last_function{};
};

}  // namespace

TEST(MpsKernelLauncherImplTest, InitializeAcquiresPipelinesInOrder) {
    mps_rt::MpsKernelLauncherImpl<2> impl({
        {"libA", "funcX"},
        {"libB", "funcY"},
    });

    DummyPrivateOps::reset();
    const base::DeviceHandle device{0};

    impl.initialize<DummyPrivateOps>(device);

    // validate size and that initialized flag is set
    EXPECT_TRUE(impl.initialized());
    EXPECT_EQ(impl.sizeForTest(), 2u);

    EXPECT_EQ(DummyPrivateOps::last_device, device);
    EXPECT_EQ(DummyPrivateOps::last_library, "libB");    // last call should be second key
    EXPECT_EQ(DummyPrivateOps::last_function, "funcY");  // last call should be second key
}

namespace {

struct MockFastOps {
    static ::orteaf::internal::backend::mps::MPSCommandBuffer_t createCommandBuffer(
        ::orteaf::internal::backend::mps::MPSCommandQueue_t command_queue) {
        last_queue = command_queue;
        return fake_buffer;
    }

    static inline ::orteaf::internal::backend::mps::MPSCommandQueue_t last_queue{nullptr};
    static inline ::orteaf::internal::backend::mps::MPSCommandBuffer_t fake_buffer{
        reinterpret_cast<::orteaf::internal::backend::mps::MPSCommandBuffer_t>(0x1)};
};

struct MockComputeFastOps {
    static ::orteaf::internal::backend::mps::MPSCommandBuffer_t createCommandBuffer(
        ::orteaf::internal::backend::mps::MPSCommandQueue_t command_queue) {
        last_queue = command_queue;
        return fake_buffer;
    }

    static ::orteaf::internal::backend::mps::MPSComputeCommandEncoder_t createComputeCommandEncoder(
        ::orteaf::internal::backend::mps::MPSCommandBuffer_t command_buffer) {
        last_command_buffer = command_buffer;
        return fake_encoder;
    }

    static void setPipelineState(::orteaf::internal::backend::mps::MPSComputeCommandEncoder_t encoder,
                                 ::orteaf::internal::backend::mps::MPSComputePipelineState_t pipeline) {
        last_encoder = encoder;
        last_pipeline = pipeline;
    }

    static void setBuffer(::orteaf::internal::backend::mps::MPSComputeCommandEncoder_t encoder,
                          ::orteaf::internal::backend::mps::MPSBuffer_t buffer,
                          std::size_t offset,
                          std::size_t index) {
        last_encoder_for_buffer = encoder;
        last_buffer = buffer;
        last_buffer_offset = offset;
        last_buffer_index = index;
    }

    static void setBytes(::orteaf::internal::backend::mps::MPSComputeCommandEncoder_t encoder,
                         const void* bytes,
                         std::size_t length,
                         std::size_t index) {
        last_encoder_for_bytes = encoder;
        last_bytes = bytes;
        last_bytes_length = length;
        last_bytes_index = index;
    }

    static void setThreadgroups(::orteaf::internal::backend::mps::MPSComputeCommandEncoder_t encoder,
                                ::orteaf::internal::backend::mps::MPSSize_t threadgroups,
                                ::orteaf::internal::backend::mps::MPSSize_t threads_per_threadgroup) {
        last_encoder = encoder;
        last_threadgroups = threadgroups;
        last_threads_per_threadgroup = threads_per_threadgroup;
    }

    static void endEncoding(::orteaf::internal::backend::mps::MPSComputeCommandEncoder_t encoder) {
        last_encoder = encoder;
    }

    static void commit(::orteaf::internal::backend::mps::MPSCommandBuffer_t command_buffer) {
        last_committed_buffer = command_buffer;
    }

    static inline ::orteaf::internal::backend::mps::MPSCommandQueue_t last_queue{nullptr};
    static inline ::orteaf::internal::backend::mps::MPSCommandBuffer_t fake_buffer{
        reinterpret_cast<::orteaf::internal::backend::mps::MPSCommandBuffer_t>(0x10)};
    static inline ::orteaf::internal::backend::mps::MPSCommandBuffer_t last_command_buffer{nullptr};
    static inline ::orteaf::internal::backend::mps::MPSComputeCommandEncoder_t fake_encoder{
        reinterpret_cast<::orteaf::internal::backend::mps::MPSComputeCommandEncoder_t>(0x20)};
    static inline ::orteaf::internal::backend::mps::MPSComputeCommandEncoder_t last_encoder{nullptr};
    static inline ::orteaf::internal::backend::mps::MPSComputePipelineState_t last_pipeline{nullptr};
    static inline ::orteaf::internal::backend::mps::MPSComputeCommandEncoder_t last_encoder_for_buffer{nullptr};
    static inline ::orteaf::internal::backend::mps::MPSBuffer_t last_buffer{nullptr};
    static inline std::size_t last_buffer_offset{0};
    static inline std::size_t last_buffer_index{0};
    static inline ::orteaf::internal::backend::mps::MPSComputeCommandEncoder_t last_encoder_for_bytes{nullptr};
    static inline const void* last_bytes{nullptr};
    static inline std::size_t last_bytes_length{0};
    static inline std::size_t last_bytes_index{0};
    static inline ::orteaf::internal::backend::mps::MPSSize_t last_threadgroups{};
    static inline ::orteaf::internal::backend::mps::MPSSize_t last_threads_per_threadgroup{};
    static inline ::orteaf::internal::backend::mps::MPSCommandBuffer_t last_committed_buffer{nullptr};
};

}  // namespace

TEST(MpsKernelLauncherImplTest, CreateCommandBufferUsesFastOps) {
    mps_rt::MpsKernelLauncherImpl<1> impl({
        {"lib", "fn"},
    });

    MockFastOps::last_queue = nullptr;
    ::orteaf::internal::backend::mps::MPSCommandQueue_t dummy_queue =
        reinterpret_cast<::orteaf::internal::backend::mps::MPSCommandQueue_t>(0x2);

    auto* buffer = impl.createCommandBuffer<MockFastOps>(dummy_queue);

    EXPECT_EQ(MockFastOps::last_queue, dummy_queue);
    EXPECT_EQ(buffer, MockFastOps::fake_buffer);
}

TEST(MpsKernelLauncherImplTest, CreateComputeEncoderBindsPipeline) {
    mps_rt::MpsKernelLauncherImpl<1> impl({
        {"lib", "fn"},
    });

    // Initialize to populate pipelines_; DummyPrivateOps returns empty leases (null pipeline is fine).
    const base::DeviceHandle device{0};
    impl.initialize<DummyPrivateOps>(device);

    MockComputeFastOps::last_command_buffer = nullptr;
    MockComputeFastOps::last_encoder = nullptr;
    MockComputeFastOps::last_pipeline = nullptr;

    auto* encoder = impl.createComputeEncoder<MockComputeFastOps>(MockComputeFastOps::fake_buffer, 0);

    EXPECT_EQ(MockComputeFastOps::last_command_buffer, MockComputeFastOps::fake_buffer);
    EXPECT_EQ(MockComputeFastOps::last_encoder, MockComputeFastOps::fake_encoder);
    EXPECT_EQ(MockComputeFastOps::last_pipeline, nullptr);  // DummyPrivateOps yields null pipeline
    EXPECT_EQ(encoder, MockComputeFastOps::fake_encoder);
}

TEST(MpsKernelLauncherImplTest, CreateComputeEncoderByNameAndIndex) {
    mps_rt::MpsKernelLauncherImpl<2> impl({
        {"libA", "fnA"},
        {"libB", "fnB"},
    });

    const base::DeviceHandle device{0};
    impl.initialize<DummyPrivateOps>(device);
    ASSERT_TRUE(impl.initialized());
    ASSERT_EQ(impl.sizeForTest(), 2u);
    ASSERT_EQ(impl.pipelineCountForTest(), 2u);
    // Ensure pipeline storage exists (DummyPrivateOps returns null pipeline, but slot is present).
    ASSERT_NO_THROW({
        auto& lease = impl.pipelineLeaseForTest(0);
        (void)lease;
    });

    MockComputeFastOps::last_command_buffer = nullptr;
    MockComputeFastOps::last_encoder = nullptr;
    MockComputeFastOps::last_pipeline = nullptr;

    // By index
    auto* enc_idx = impl.createComputeEncoder<MockComputeFastOps>(MockComputeFastOps::fake_buffer, 1);
    EXPECT_EQ(enc_idx, MockComputeFastOps::fake_encoder);
    EXPECT_EQ(MockComputeFastOps::last_command_buffer, MockComputeFastOps::fake_buffer);
    EXPECT_EQ(MockComputeFastOps::last_pipeline, nullptr);

    // By name
    MockComputeFastOps::last_command_buffer = nullptr;
    MockComputeFastOps::last_encoder = nullptr;
    MockComputeFastOps::last_pipeline = nullptr;

    auto* enc_name = impl.createComputeEncoder<MockComputeFastOps>(MockComputeFastOps::fake_buffer,
                                                                   "libA", "fnA");
    EXPECT_EQ(enc_name, MockComputeFastOps::fake_encoder);
    EXPECT_EQ(MockComputeFastOps::last_command_buffer, MockComputeFastOps::fake_buffer);
    EXPECT_EQ(MockComputeFastOps::last_pipeline, nullptr);

    // Missing pipeline returns nullptr and does not configure encoder
    MockComputeFastOps::last_encoder = nullptr;
    auto* enc_missing = impl.createComputeEncoder<MockComputeFastOps>(MockComputeFastOps::fake_buffer,
                                                                      "missing", "missing");
    EXPECT_EQ(enc_missing, nullptr);
}

TEST(MpsKernelLauncherImplTest, EncoderSetBufferAndBytesForwarded) {
    mps_rt::MpsKernelLauncherImpl<1> impl({
        {"lib", "fn"},
    });

    // We don't need real pipelines; just need the helpers callable.
    const base::DeviceHandle device{0};
    impl.initialize<DummyPrivateOps>(device);

    auto* encoder = MockComputeFastOps::fake_encoder;
    auto* buffer = reinterpret_cast<::orteaf::internal::backend::mps::MPSBuffer_t>(0x30);
    constexpr std::size_t kOffset = 16;
    constexpr std::size_t kBufIndex = 2;

    MockComputeFastOps::last_encoder_for_buffer = nullptr;
    MockComputeFastOps::last_buffer = nullptr;
    MockComputeFastOps::last_buffer_offset = 0;
    MockComputeFastOps::last_buffer_index = 0;

    impl.setBuffer<MockComputeFastOps>(encoder, buffer, kOffset, kBufIndex);

    EXPECT_EQ(MockComputeFastOps::last_encoder_for_buffer, encoder);
    EXPECT_EQ(MockComputeFastOps::last_buffer, buffer);
    EXPECT_EQ(MockComputeFastOps::last_buffer_offset, kOffset);
    EXPECT_EQ(MockComputeFastOps::last_buffer_index, kBufIndex);

    // Bytes
    int payload = 42;
    constexpr std::size_t kBytesIndex = 3;
    MockComputeFastOps::last_encoder_for_bytes = nullptr;
    MockComputeFastOps::last_bytes = nullptr;
    MockComputeFastOps::last_bytes_length = 0;
    MockComputeFastOps::last_bytes_index = 0;

    impl.setBytes<MockComputeFastOps>(encoder, &payload, sizeof(payload), kBytesIndex);

    EXPECT_EQ(MockComputeFastOps::last_encoder_for_bytes, encoder);
    EXPECT_EQ(MockComputeFastOps::last_bytes, &payload);
    EXPECT_EQ(MockComputeFastOps::last_bytes_length, sizeof(payload));
    EXPECT_EQ(MockComputeFastOps::last_bytes_index, kBytesIndex);
}

TEST(MpsKernelLauncherImplTest, DispatchEndCommitForwarded) {
    mps_rt::MpsKernelLauncherImpl<1> impl({
        {"lib", "fn"},
    });

    const base::DeviceHandle device{0};
    impl.initialize<DummyPrivateOps>(device);

    MockComputeFastOps::last_encoder = nullptr;
    MockComputeFastOps::last_threadgroups = {};
    MockComputeFastOps::last_threads_per_threadgroup = {};
    MockComputeFastOps::last_committed_buffer = nullptr;

    ::orteaf::internal::backend::mps::MPSSize_t tg{1, 2, 3};
    ::orteaf::internal::backend::mps::MPSSize_t tptg{4, 5, 6};

    auto* encoder = MockComputeFastOps::fake_encoder;
    auto* command_buffer = MockComputeFastOps::fake_buffer;

    impl.dispatchThreadgroups<MockComputeFastOps>(encoder, tg, tptg);
    impl.endEncoding<MockComputeFastOps>(encoder);
    impl.commit<MockComputeFastOps>(command_buffer);

    EXPECT_EQ(MockComputeFastOps::last_encoder, encoder);
    EXPECT_EQ(MockComputeFastOps::last_threadgroups.width, tg.width);
    EXPECT_EQ(MockComputeFastOps::last_threadgroups.height, tg.height);
    EXPECT_EQ(MockComputeFastOps::last_threadgroups.depth, tg.depth);
    EXPECT_EQ(MockComputeFastOps::last_threads_per_threadgroup.width, tptg.width);
    EXPECT_EQ(MockComputeFastOps::last_threads_per_threadgroup.height, tptg.height);
    EXPECT_EQ(MockComputeFastOps::last_threads_per_threadgroup.depth, tptg.depth);
    EXPECT_EQ(MockComputeFastOps::last_committed_buffer, command_buffer);
}

TEST(MpsKernelLauncherImplTest, DispatchOneShotByIndex) {
    mps_rt::MpsKernelLauncherImpl<1> impl({
        {"lib", "fn"},
    });

    const base::DeviceHandle device{0};
    impl.initialize<DummyPrivateOps>(device);

    ::orteaf::internal::backend::mps::MPSCommandQueue_t queue =
        reinterpret_cast<::orteaf::internal::backend::mps::MPSCommandQueue_t>(0x40);
    ::orteaf::internal::backend::mps::MPSSize_t tg{7, 8, 9};
    ::orteaf::internal::backend::mps::MPSSize_t tptg{1, 2, 3};

    MockComputeFastOps::last_queue = nullptr;
    MockComputeFastOps::last_command_buffer = nullptr;
    MockComputeFastOps::last_encoder = nullptr;
    MockComputeFastOps::last_pipeline = nullptr;
    MockComputeFastOps::last_threadgroups = {};
    MockComputeFastOps::last_threads_per_threadgroup = {};
    MockComputeFastOps::last_committed_buffer = nullptr;

    bool binder_called = false;
    auto binder = [&](auto* encoder) {
        binder_called = true;
        EXPECT_EQ(encoder, MockComputeFastOps::fake_encoder);
    };

    auto* command_buffer = impl.dispatchOneShot<MockComputeFastOps>(queue, 0, tg, tptg, binder);

    EXPECT_TRUE(binder_called);
    EXPECT_EQ(command_buffer, MockComputeFastOps::fake_buffer);
    EXPECT_EQ(MockComputeFastOps::last_queue, queue);
    EXPECT_EQ(MockComputeFastOps::last_command_buffer, MockComputeFastOps::fake_buffer);
    EXPECT_EQ(MockComputeFastOps::last_encoder, MockComputeFastOps::fake_encoder);
    EXPECT_EQ(MockComputeFastOps::last_pipeline, nullptr);  // DummyPrivateOps yields null pipeline
    EXPECT_EQ(MockComputeFastOps::last_threadgroups.width, tg.width);
    EXPECT_EQ(MockComputeFastOps::last_threads_per_threadgroup.width, tptg.width);
    EXPECT_EQ(MockComputeFastOps::last_committed_buffer, MockComputeFastOps::fake_buffer);
}

TEST(MpsKernelLauncherImplTest, DispatchOneShotByNameMissingReturnsNullptr) {
    mps_rt::MpsKernelLauncherImpl<1> impl({
        {"lib", "fn"},
    });
    const base::DeviceHandle device{0};
    impl.initialize<DummyPrivateOps>(device);

    ::orteaf::internal::backend::mps::MPSCommandQueue_t queue =
        reinterpret_cast<::orteaf::internal::backend::mps::MPSCommandQueue_t>(0x40);
    ::orteaf::internal::backend::mps::MPSSize_t tg{1, 1, 1};
    ::orteaf::internal::backend::mps::MPSSize_t tptg{1, 1, 1};

    auto* command_buffer = impl.dispatchOneShot<MockComputeFastOps>(queue, "missing", "missing", tg, tptg,
                                                                    [](auto*) {});
    EXPECT_EQ(command_buffer, nullptr);
}

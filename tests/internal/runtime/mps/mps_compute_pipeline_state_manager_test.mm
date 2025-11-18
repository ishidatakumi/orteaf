#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <limits>
#include <optional>
#include <string>
#include <system_error>

#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/runtime/manager/mps/mps_compute_pipeline_state_manager.h"
#include "tests/internal/runtime/mps/testing/backend_ops_provider.h"
#include "tests/internal/runtime/mps/testing/manager_test_fixture.h"
#include "tests/internal/testing/error_assert.h"

namespace backend = orteaf::internal::backend;
namespace diag_error = orteaf::internal::diagnostics::error;
namespace base = orteaf::internal::base;
namespace mps_rt = orteaf::internal::runtime::mps;
namespace testing_mps = orteaf::tests::runtime::mps::testing;
using orteaf::tests::ExpectError;

#define ORTEAF_MPS_ENV_LIBRARY_NAME "ORTEAF_EXPECT_MPS_LIBRARY_NAME"
#define ORTEAF_MPS_ENV_FUNCTION_NAME "ORTEAF_EXPECT_MPS_FUNCTION_NAME"

namespace {

backend::mps::MPSLibrary_t makeLibrary(std::uintptr_t value) {
    return reinterpret_cast<backend::mps::MPSLibrary_t>(value);
}

backend::mps::MPSFunction_t makeFunction(std::uintptr_t value) {
    return reinterpret_cast<backend::mps::MPSFunction_t>(value);
}

backend::mps::MPSComputePipelineState_t makePipeline(std::uintptr_t value) {
    return reinterpret_cast<backend::mps::MPSComputePipelineState_t>(value);
}

template <class Provider>
class MpsComputePipelineStateManagerTypedTest
    : public testing_mps::RuntimeManagerFixture<Provider, mps_rt::MpsComputePipelineStateManager> {
protected:
    using Base = testing_mps::RuntimeManagerFixture<Provider, mps_rt::MpsComputePipelineStateManager>;

    mps_rt::MpsComputePipelineStateManager<typename Provider::BackendOps>& manager() {
        return Base::manager();
    }

    auto& adapter() { return Base::adapter(); }

    void TearDown() override {
        manager().shutdown();
        if (library_ != nullptr) {
            if constexpr (Provider::is_mock) {
                adapter().expectDestroyLibraries({library_});
            }
            Provider::BackendOps::destroyLibrary(library_);
            library_ = nullptr;
        }
        Base::TearDown();
    }

    bool initializeManager(std::size_t capacity = 0) {
        const auto device = adapter().device();
        if (auto library = ensureLibrary()) {
            manager().initialize(device, *library, capacity);
            return true;
        }
        return false;
    }

    std::optional<std::string> libraryNameFromEnv() const {
        if constexpr (Provider::is_mock) {
            return std::string{"MockLibrary"};
        }
        const char* value = std::getenv(ORTEAF_MPS_ENV_LIBRARY_NAME);
        if (value == nullptr || *value == '\0') {
            return std::nullopt;
        }
        return std::string{value};
    }

    std::optional<std::string> functionNameFromEnv() const {
        if constexpr (Provider::is_mock) {
            return std::string{"MockFunction"};
        }
        const char* value = std::getenv(ORTEAF_MPS_ENV_FUNCTION_NAME);
        if (value == nullptr || *value == '\0') {
            return std::nullopt;
        }
        return std::string{value};
    }

    std::optional<backend::mps::MPSLibrary_t> ensureLibrary() {
        if (library_ != nullptr) {
            return library_;
        }
        const auto maybe_name = libraryNameFromEnv();
        if (!maybe_name.has_value()) {
            skipMissingLibraryEnv();
            return std::nullopt;
        }
        const auto device = adapter().device();
        if constexpr (Provider::is_mock) {
            const auto expected = makeLibrary(0x701);
            adapter().expectCreateLibraries({{*maybe_name, expected}});
        }
        library_ = Provider::BackendOps::createLibraryWithName(device, *maybe_name);
        return library_;
    }

    void skipMissingLibraryEnv() const {
        GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_LIBRARY_NAME " to a valid library name to run";
    }

private:
    backend::mps::MPSLibrary_t library_{nullptr};
};

#if ORTEAF_ENABLE_MPS
using ProviderTypes = ::testing::Types<
    testing_mps::MockBackendOpsProvider,
    testing_mps::RealBackendOpsProvider>;
#else
using ProviderTypes = ::testing::Types<
    testing_mps::MockBackendOpsProvider>;
#endif

}  // namespace

TYPED_TEST_SUITE(MpsComputePipelineStateManagerTypedTest, ProviderTypes);

TYPED_TEST(MpsComputePipelineStateManagerTypedTest, GrowthChunkSizeCanBeAdjusted) {
    auto& manager = this->manager();
    EXPECT_EQ(manager.growthChunkSize(), 1u);
    manager.setGrowthChunkSize(5);
    EXPECT_EQ(manager.growthChunkSize(), 5u);
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest, GrowthChunkSizeRejectsZero) {
    auto& manager = this->manager();
    ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] { manager.setGrowthChunkSize(0); });
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest, GrowthChunkSizeControlsPoolExpansion) {
    auto& manager = this->manager();
    if constexpr (!TypeParam::is_mock) {
        GTEST_SKIP() << "Mock-only test";
        return;
    }
    manager.setGrowthChunkSize(2);
    if (!this->initializeManager(0)) {
        return;
    }

    const auto key = mps_rt::FunctionKey::Named("ChunkedFunction");
    const auto function_handle = makeFunction(0x8801);
    const auto pipeline_handle = makePipeline(0x9901);
    this->adapter().expectCreateFunctions({{"ChunkedFunction", function_handle}});
    this->adapter().expectCreateComputePipelineStates({{function_handle, pipeline_handle}});

    const auto id = manager.getOrCreate(key);
    EXPECT_EQ(manager.capacity(), 2u);
    const auto snapshot = manager.debugState(id);
    EXPECT_EQ(snapshot.growth_chunk_size, 2u);

    this->adapter().expectDestroyComputePipelineStates({pipeline_handle});
    this->adapter().expectDestroyFunctions({function_handle});
    manager.release(id);
    manager.shutdown();
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest, AccessBeforeInitializationThrows) {
    auto& manager = this->manager();
    const auto key = mps_rt::FunctionKey::Named("Unused");
    ExpectError(diag_error::OrteafErrc::InvalidState, [&] { (void)manager.getOrCreate(key); });
    ExpectError(diag_error::OrteafErrc::InvalidState, [&] { manager.release(base::FunctionId{0}); });
    ExpectError(diag_error::OrteafErrc::InvalidState, [&] { (void)manager.getPipelineState(base::FunctionId{0}); });
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest, InitializeRejectsNullDevice) {
    auto& manager = this->manager();
    const auto maybe_library = this->ensureLibrary();
    if (!maybe_library.has_value()) {
        return;
    }
    ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] { manager.initialize(nullptr, *maybe_library, 1); });
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest, InitializeRejectsNullLibrary) {
    auto& manager = this->manager();
    const auto device = this->adapter().device();
    ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] { manager.initialize(device, nullptr, 1); });
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest, InitializeRejectsCapacityAboveLimit) {
    auto& manager = this->manager();
    const auto device = this->adapter().device();
    const auto maybe_library = this->ensureLibrary();
    if (!maybe_library.has_value()) {
        return;
    }
    ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
        manager.initialize(device, *maybe_library, std::numeric_limits<std::size_t>::max());
    });
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest, InitializeWithZeroCapacityIsAllowed) {
    auto& manager = this->manager();
    if (!this->initializeManager(0)) {
        return;
    }
    EXPECT_EQ(manager.capacity(), 0u);
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest, InitializeSetsCapacity) {
    auto& manager = this->manager();
    if (!this->initializeManager(2)) {
        return;
    }
    EXPECT_EQ(manager.capacity(), 2u);
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest, GetOrCreateCreatesAndCachesPipeline) {
    auto& manager = this->manager();
    if (!this->initializeManager()) {
        return;
    }

    const auto maybe_name = this->functionNameFromEnv();
    if (!maybe_name.has_value()) {
        GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_FUNCTION_NAME " to a valid function name to run";
        return;
    }
    backend::mps::MPSFunction_t function_handle = nullptr;
    backend::mps::MPSComputePipelineState_t pipeline_handle = nullptr;
    if constexpr (TypeParam::is_mock) {
        function_handle = makeFunction(0x801);
        pipeline_handle = makePipeline(0x901);
        this->adapter().expectCreateFunctions({{*maybe_name, function_handle}});
        this->adapter().expectCreateComputePipelineStates({{function_handle, pipeline_handle}});
    }

    const auto key = mps_rt::FunctionKey::Named(*maybe_name);
    const auto id0 = manager.getOrCreate(key);
    const auto id1 = manager.getOrCreate(key);
    EXPECT_EQ(id0, id1);
    if constexpr (TypeParam::is_mock) {
        EXPECT_EQ(manager.getPipelineState(id0), pipeline_handle);
    } else {
        EXPECT_NE(manager.getPipelineState(id0), nullptr);
    }

    const auto snapshot = manager.debugState(id0);
    EXPECT_TRUE(snapshot.alive);
    EXPECT_TRUE(snapshot.pipeline_allocated);
    EXPECT_TRUE(snapshot.function_allocated);
    EXPECT_EQ(snapshot.identifier, *maybe_name);
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest, FunctionCreationFailureIsReported) {
    auto& manager = this->manager();
    if (!this->initializeManager()) {
        return;
    }
    if constexpr (!TypeParam::is_mock) {
        GTEST_SKIP() << "Function creation failure scenario only applies to mocks";
        return;
    }
    const auto maybe_name = this->functionNameFromEnv();
    if (!maybe_name.has_value()) {
        GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_FUNCTION_NAME " to a valid function name to run";
        return;
    }
    this->adapter().expectCreateFunctions({{*maybe_name, nullptr}});
    ExpectError(diag_error::OrteafErrc::InvalidState, [&] {
        (void)manager.getOrCreate(mps_rt::FunctionKey::Named(*maybe_name));
    });
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest, PipelineCreationFailureDestroysFunction) {
    auto& manager = this->manager();
    if (!this->initializeManager()) {
        return;
    }
    if constexpr (!TypeParam::is_mock) {
        GTEST_SKIP() << "Pipeline creation failure scenario only applies to mocks";
        return;
    }
    const auto maybe_name = this->functionNameFromEnv();
    if (!maybe_name.has_value()) {
        GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_FUNCTION_NAME " to a valid function name to run";
        return;
    }
    const auto function_handle = makeFunction(0x811);
    this->adapter().expectCreateFunctions({{*maybe_name, function_handle}});
    this->adapter().expectCreateComputePipelineStates({{function_handle, nullptr}});
    this->adapter().expectDestroyFunctions({function_handle});
    ExpectError(diag_error::OrteafErrc::InvalidState, [&] {
        (void)manager.getOrCreate(mps_rt::FunctionKey::Named(*maybe_name));
    });
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest, ReleaseDestroysHandlesAndAllowsRecreation) {
    auto& manager = this->manager();
    if (!this->initializeManager()) {
        return;
    }

    const auto maybe_name = this->functionNameFromEnv();
    if (!maybe_name.has_value()) {
        GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_FUNCTION_NAME " to a valid function name to run";
        return;
    }
    backend::mps::MPSFunction_t first_function = nullptr;
    backend::mps::MPSComputePipelineState_t first_pipeline = nullptr;
    backend::mps::MPSFunction_t second_function = nullptr;
    backend::mps::MPSComputePipelineState_t second_pipeline = nullptr;
    if constexpr (TypeParam::is_mock) {
        first_function = makeFunction(0x820);
        first_pipeline = makePipeline(0x920);
        second_function = makeFunction(0x821);
        second_pipeline = makePipeline(0x921);
        this->adapter().expectCreateFunctions({{*maybe_name, first_function}, {*maybe_name, second_function}});
        this->adapter().expectCreateComputePipelineStates({
            {first_function, first_pipeline},
            {second_function, second_pipeline},
        });
        this->adapter().expectDestroyComputePipelineStates({first_pipeline, second_pipeline});
        this->adapter().expectDestroyFunctions({first_function, second_function});
    }

    const auto key = mps_rt::FunctionKey::Named(*maybe_name);
    const auto id = manager.getOrCreate(key);
    manager.release(id);
    ExpectError(diag_error::OrteafErrc::InvalidState, [&] { (void)manager.getPipelineState(id); });
    const auto released_snapshot = manager.debugState(id);
    EXPECT_FALSE(released_snapshot.alive);

    const auto reacquired = manager.getOrCreate(key);
    EXPECT_NE(reacquired, base::FunctionId{});
    if constexpr (TypeParam::is_mock) {
        EXPECT_EQ(manager.getPipelineState(reacquired), second_pipeline);
    } else {
        EXPECT_NE(manager.getPipelineState(reacquired), nullptr);
    }
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest, EmptyIdentifierIsRejected) {
    auto& manager = this->manager();
    if (!this->initializeManager()) {
        return;
    }
    ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
        (void)manager.getOrCreate(mps_rt::FunctionKey::Named(""));
    });
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest, ReleaseRejectsStaleId) {
    auto& manager = this->manager();
    if (!this->initializeManager()) {
        return;
    }

    const auto maybe_name = this->functionNameFromEnv();
    if (!maybe_name.has_value()) {
        GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_FUNCTION_NAME " to a valid function name to run";
        return;
    }
    backend::mps::MPSFunction_t function_handle = nullptr;
    backend::mps::MPSComputePipelineState_t pipeline_handle = nullptr;
    if constexpr (TypeParam::is_mock) {
        function_handle = makeFunction(0x830);
        pipeline_handle = makePipeline(0x930);
        this->adapter().expectCreateFunctions({{*maybe_name, function_handle}});
        this->adapter().expectCreateComputePipelineStates({{function_handle, pipeline_handle}});
        this->adapter().expectDestroyComputePipelineStates({pipeline_handle});
        this->adapter().expectDestroyFunctions({function_handle});
    }

    const auto key = mps_rt::FunctionKey::Named(*maybe_name);
    const auto id = manager.getOrCreate(key);
    manager.release(id);
    ExpectError(diag_error::OrteafErrc::InvalidState, [&] { manager.release(id); });
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest, GetPipelineStateRejectsInvalidId) {
    auto& manager = this->manager();
    if (!this->initializeManager()) {
        return;
    }
    ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
        (void)manager.getPipelineState(base::FunctionId{std::numeric_limits<std::uint32_t>::max()});
    });
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest, ShutdownDestroysAllHandles) {
    auto& manager = this->manager();
    if (!this->initializeManager()) {
        return;
    }
    const auto maybe_name = this->functionNameFromEnv();
    if (!maybe_name.has_value()) {
        GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_FUNCTION_NAME " to a valid function name to run";
        return;
    }
    backend::mps::MPSFunction_t function_handle = nullptr;
    backend::mps::MPSComputePipelineState_t pipeline_handle = nullptr;
    if constexpr (TypeParam::is_mock) {
        function_handle = makeFunction(0x840);
        pipeline_handle = makePipeline(0x940);
        this->adapter().expectCreateFunctions({{*maybe_name, function_handle}});
        this->adapter().expectCreateComputePipelineStates({{function_handle, pipeline_handle}});
        this->adapter().expectDestroyComputePipelineStates({pipeline_handle});
        this->adapter().expectDestroyFunctions({function_handle});
    }
    const auto key = mps_rt::FunctionKey::Named(*maybe_name);
    (void)manager.getOrCreate(key);
    manager.shutdown();
    ExpectError(diag_error::OrteafErrc::InvalidState, [&] { (void)manager.getOrCreate(key); });
}

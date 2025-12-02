#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <limits>
#include <optional>
#include <string>
#include <system_error>

#include <orteaf/internal/diagnostics/error/error.h>
#include <orteaf/internal/runtime/manager/mps/mps_library_manager.h>
#include <tests/internal/runtime/manager/mps/testing/backend_ops_provider.h>
#include <tests/internal/runtime/manager/mps/testing/manager_test_fixture.h>
#include <tests/internal/testing/error_assert.h>

namespace backend = orteaf::internal::backend;
namespace base = orteaf::internal::base;
namespace diag_error = orteaf::internal::diagnostics::error;
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
class MpsLibraryManagerTypedTest
    : public testing_mps::RuntimeManagerFixture<Provider, mps_rt::MpsLibraryManager> {
protected:
    using Base = testing_mps::RuntimeManagerFixture<Provider, mps_rt::MpsLibraryManager>;

    mps_rt::MpsLibraryManager& manager() {
        return Base::manager();
    }

    auto& adapter() { return Base::adapter(); }

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

    void initializeManager(std::size_t capacity = 0) {
        const auto device = adapter().device();
        manager().initialize(device, this->getOps(), capacity);
    }

    void TearDown() override {
        manager().shutdown();
        Base::TearDown();
    }
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

TYPED_TEST_SUITE(MpsLibraryManagerTypedTest, ProviderTypes);

TYPED_TEST(MpsLibraryManagerTypedTest, GrowthChunkSizeCanBeAdjusted) {
    auto& manager = this->manager();
    EXPECT_EQ(manager.growthChunkSize(), 1u);
    manager.setGrowthChunkSize(4);
    EXPECT_EQ(manager.growthChunkSize(), 4u);
}

TYPED_TEST(MpsLibraryManagerTypedTest, GrowthChunkSizeRejectsZero) {
    auto& manager = this->manager();
    ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] { manager.setGrowthChunkSize(0); });
}

TYPED_TEST(MpsLibraryManagerTypedTest, GrowthChunkSizeControlsPoolExpansion) {
    auto& manager = this->manager();
    if constexpr (!TypeParam::is_mock) {
        GTEST_SKIP() << "Mock-only test";
        return;
    }
    manager.setGrowthChunkSize(3);
    const auto device = this->adapter().device();
    manager.initialize(device, this->getOps(), 0);

    const auto key = mps_rt::LibraryKey::Named("ChunkedLibrary");
    const auto handle = makeLibrary(0x3501);
    this->adapter().expectCreateLibraries({{"ChunkedLibrary", handle}});
    auto lease = manager.acquire(key);
    EXPECT_EQ(manager.capacity(), 3u);
    const auto snapshot = manager.debugState(lease.handle());
    EXPECT_EQ(snapshot.growth_chunk_size, 3u);
    this->adapter().expectDestroyLibraries({handle});
    lease.release();
    manager.shutdown();
}

TYPED_TEST(MpsLibraryManagerTypedTest, AccessBeforeInitializationThrows) {
    auto& manager = this->manager();
    const auto key = mps_rt::LibraryKey::Named("Unused");

    ExpectError(diag_error::OrteafErrc::InvalidState, [&] { (void)manager.acquire(key); });
}

TYPED_TEST(MpsLibraryManagerTypedTest, InitializeRejectsNullDevice) {
    auto& manager = this->manager();
    ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] { manager.initialize(nullptr, this->getOps(), 1); });
}

TYPED_TEST(MpsLibraryManagerTypedTest, InitializeRejectsCapacityAboveLimit) {
    auto& manager = this->manager();
    const auto device = this->adapter().device();
    ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
        manager.initialize(device, this->getOps(), std::numeric_limits<std::size_t>::max());
    });
}

TYPED_TEST(MpsLibraryManagerTypedTest, InitializeWithZeroCapacityIsAllowed) {
    auto& manager = this->manager();
    this->initializeManager(0);
    EXPECT_EQ(manager.capacity(), 0u);
}

TYPED_TEST(MpsLibraryManagerTypedTest, InitializeSetsCapacity) {
    auto& manager = this->manager();
    this->initializeManager(3);
    EXPECT_EQ(manager.capacity(), 3u);
}

TYPED_TEST(MpsLibraryManagerTypedTest, GetOrCreateAllocatesAndCachesLibrary) {
    auto& manager = this->manager();
    this->initializeManager();

    const auto maybe_name = this->libraryNameFromEnv();
    if (!maybe_name.has_value()) {
        GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_LIBRARY_NAME " to a valid library name to run";
        return;
    }
    const auto key = mps_rt::LibraryKey::Named(*maybe_name);
    backend::mps::MPSLibrary_t expected = nullptr;
    if constexpr (TypeParam::is_mock) {
        expected = makeLibrary(0x501);
        this->adapter().expectCreateLibraries({{*maybe_name, expected}});
    }

    auto lease0 = manager.acquire(key);
    auto lease1 = manager.acquire(key);
    EXPECT_EQ(lease0.handle(), lease1.handle());
    if constexpr (TypeParam::is_mock) {
        EXPECT_EQ(lease0.get(), expected);
    } else {
        EXPECT_NE(lease0.get(), nullptr);
    }

    const auto snapshot = manager.debugState(lease0.handle());
    EXPECT_TRUE(snapshot.alive);
    EXPECT_TRUE(snapshot.handle_allocated);
    EXPECT_EQ(snapshot.identifier, *maybe_name);
    lease1.release();
    lease0.release();
}

TYPED_TEST(MpsLibraryManagerTypedTest, ReleaseDestroysHandleAndAllowsRecreation) {
    auto& manager = this->manager();
    this->initializeManager();
    const auto maybe_name = this->libraryNameFromEnv();
    if (!maybe_name.has_value()) {
        GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_LIBRARY_NAME " to a valid library name to run";
        return;
    }
    const auto key = mps_rt::LibraryKey::Named(*maybe_name);

    backend::mps::MPSLibrary_t first_handle = nullptr;
    backend::mps::MPSLibrary_t second_handle = nullptr;
    if constexpr (TypeParam::is_mock) {
        first_handle = makeLibrary(0x600);
        second_handle = makeLibrary(0x601);
        this->adapter().expectCreateLibraries({{*maybe_name, first_handle}, {*maybe_name, second_handle}});
        this->adapter().expectDestroyLibraries({first_handle});
        this->adapter().expectDestroyLibraries({second_handle});
    }

    auto lease = manager.acquire(key);
    const auto released_handle = lease.handle();
    lease.release();
    const auto released_snapshot = manager.debugState(released_handle);
    EXPECT_FALSE(released_snapshot.alive);
    EXPECT_FALSE(released_snapshot.handle_allocated);

    auto reacquired = manager.acquire(key);
    EXPECT_NE(reacquired.handle(), base::LibraryHandle{});
    if constexpr (TypeParam::is_mock) {
        EXPECT_EQ(reacquired.get(), second_handle);
    } else {
        EXPECT_NE(reacquired.get(), nullptr);
    }
    reacquired.release();
}

TYPED_TEST(MpsLibraryManagerTypedTest, EmptyIdentifierIsRejected) {
    auto& manager = this->manager();
    this->initializeManager();
    ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
        (void)manager.acquire(mps_rt::LibraryKey::Named(""));
    });
}

TYPED_TEST(MpsLibraryManagerTypedTest, ReleaseRejectsStaleId) {
    auto& manager = this->manager();
    this->initializeManager();
    const auto maybe_name = this->libraryNameFromEnv();
    if (!maybe_name.has_value()) {
        GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_LIBRARY_NAME " to a valid library name to run";
        return;
    }
    const auto key = mps_rt::LibraryKey::Named(*maybe_name);

    backend::mps::MPSLibrary_t handle = nullptr;
    if constexpr (TypeParam::is_mock) {
        handle = makeLibrary(0x650);
        this->adapter().expectCreateLibraries({{*maybe_name, handle}});
        this->adapter().expectDestroyLibraries({handle});
    }

    auto lease = manager.acquire(key);
    const auto handle_id = lease.handle();
    lease.release();
    lease.release();  // idempotent
    const auto snapshot = manager.debugState(handle_id);
    EXPECT_FALSE(snapshot.alive);
}

TYPED_TEST(MpsLibraryManagerTypedTest, PipelineManagerProvidesNestedFunctionManager) {
    auto& manager = this->manager();
    this->initializeManager();

    const auto maybe_library = this->libraryNameFromEnv();
    if (!maybe_library.has_value()) {
        GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_LIBRARY_NAME " to a valid library name to run";
        return;
    }
    const auto maybe_function = this->functionNameFromEnv();
    if (!maybe_function.has_value()) {
        GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_FUNCTION_NAME " to a valid function name to run";
        return;
    }

    backend::mps::MPSLibrary_t library_handle = nullptr;
    backend::mps::MPSFunction_t function_handle = nullptr;
    backend::mps::MPSComputePipelineState_t pipeline_handle = nullptr;
    if constexpr (TypeParam::is_mock) {
        library_handle = makeLibrary(0x670);
        function_handle = makeFunction(0x770);
        pipeline_handle = makePipeline(0x870);
        this->adapter().expectCreateLibraries({{*maybe_library, library_handle}});
        this->adapter().expectCreateFunctions({{*maybe_function, function_handle}});
        this->adapter().expectCreateComputePipelineStates({{function_handle, pipeline_handle}});
        this->adapter().expectDestroyComputePipelineStates({pipeline_handle});
        this->adapter().expectDestroyFunctions({function_handle});
        this->adapter().expectDestroyLibraries({library_handle});
    }

    auto library_lease = manager.acquire(mps_rt::LibraryKey::Named(*maybe_library));
    auto pipeline_manager_lease = manager.acquirePipelineManager(library_lease);
    auto* pipeline_manager = pipeline_manager_lease.get();
    const auto pipeline_id = pipeline_manager->getOrCreate(mps_rt::FunctionKey::Named(*maybe_function));
    if constexpr (TypeParam::is_mock) {
        EXPECT_EQ(pipeline_manager->getPipelineState(pipeline_id), pipeline_handle);
    } else {
        EXPECT_NE(pipeline_manager->getPipelineState(pipeline_id), nullptr);
    }
    const auto snapshot = pipeline_manager->debugState(pipeline_id);
    EXPECT_TRUE(snapshot.alive);
    EXPECT_EQ(snapshot.identifier, *maybe_function);
    pipeline_manager_lease.release();
    library_lease.release();
}

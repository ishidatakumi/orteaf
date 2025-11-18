#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <limits>
#include <optional>
#include <string>
#include <system_error>

#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/runtime/manager/mps/mps_library_manager.h"
#include "tests/internal/runtime/mps/testing/backend_ops_provider.h"
#include "tests/internal/runtime/mps/testing/manager_test_fixture.h"
#include "tests/internal/testing/error_assert.h"

namespace backend = orteaf::internal::backend;
namespace base = orteaf::internal::base;
namespace diag_error = orteaf::internal::diagnostics::error;
namespace mps_rt = orteaf::internal::runtime::mps;
namespace testing_mps = orteaf::tests::runtime::mps::testing;
using orteaf::tests::ExpectError;

#define ORTEAF_MPS_ENV_LIBRARY_NAME "ORTEAF_EXPECT_MPS_LIBRARY_NAME"

namespace {

backend::mps::MPSLibrary_t makeLibrary(std::uintptr_t value) {
    return reinterpret_cast<backend::mps::MPSLibrary_t>(value);
}

template <class Provider>
class MpsLibraryManagerTypedTest
    : public testing_mps::RuntimeManagerFixture<Provider, mps_rt::MpsLibraryManager> {
protected:
    using Base = testing_mps::RuntimeManagerFixture<Provider, mps_rt::MpsLibraryManager>;

    mps_rt::MpsLibraryManager<typename Provider::BackendOps>& manager() {
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

    void initializeManager(std::size_t capacity = 0) {
        const auto device = adapter().device();
        manager().initialize(device, capacity);
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

TYPED_TEST(MpsLibraryManagerTypedTest, AccessBeforeInitializationThrows) {
    auto& manager = this->manager();
    const auto key = mps_rt::LibraryKey::Named("Unused");

    ExpectError(diag_error::OrteafErrc::InvalidState, [&] { (void)manager.getOrCreate(key); });
    ExpectError(diag_error::OrteafErrc::InvalidState, [&] { manager.release(base::LibraryId{0}); });
    ExpectError(diag_error::OrteafErrc::InvalidState, [&] { (void)manager.getLibrary(base::LibraryId{0}); });
}

TYPED_TEST(MpsLibraryManagerTypedTest, InitializeRejectsNullDevice) {
    auto& manager = this->manager();
    ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] { manager.initialize(nullptr, 1); });
}

TYPED_TEST(MpsLibraryManagerTypedTest, InitializeRejectsCapacityAboveLimit) {
    auto& manager = this->manager();
    const auto device = this->adapter().device();
    ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
        manager.initialize(device, std::numeric_limits<std::size_t>::max());
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
    }
    const auto key = mps_rt::LibraryKey::Named(*maybe_name);
    backend::mps::MPSLibrary_t expected = nullptr;
    if constexpr (TypeParam::is_mock) {
        expected = makeLibrary(0x501);
        this->adapter().expectCreateLibraries({{*maybe_name, expected}});
    }

    const auto id0 = manager.getOrCreate(key);
    const auto id1 = manager.getOrCreate(key);
    EXPECT_EQ(id0, id1);
    if constexpr (TypeParam::is_mock) {
        EXPECT_EQ(manager.getLibrary(id0), expected);
    } else {
        EXPECT_NE(manager.getLibrary(id0), nullptr);
    }

    const auto snapshot = manager.debugState(id0);
    EXPECT_TRUE(snapshot.alive);
    EXPECT_TRUE(snapshot.handle_allocated);
    EXPECT_EQ(snapshot.identifier, *maybe_name);
}

TYPED_TEST(MpsLibraryManagerTypedTest, ReleaseDestroysHandleAndAllowsRecreation) {
    auto& manager = this->manager();
    this->initializeManager();
    const auto maybe_name = this->libraryNameFromEnv();
    if (!maybe_name.has_value()) {
        GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_LIBRARY_NAME " to a valid library name to run";
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

    const auto id = manager.getOrCreate(key);
    manager.release(id);
    ExpectError(diag_error::OrteafErrc::InvalidState, [&] { (void)manager.getLibrary(id); });
    const auto released_snapshot = manager.debugState(id);
    EXPECT_FALSE(released_snapshot.alive);
    EXPECT_FALSE(released_snapshot.handle_allocated);

    const auto reacquired = manager.getOrCreate(key);
    EXPECT_NE(reacquired, base::LibraryId{});
    if constexpr (TypeParam::is_mock) {
        EXPECT_EQ(manager.getLibrary(reacquired), second_handle);
    } else {
        EXPECT_NE(manager.getLibrary(reacquired), nullptr);
    }
}

TYPED_TEST(MpsLibraryManagerTypedTest, EmptyIdentifierIsRejected) {
    auto& manager = this->manager();
    this->initializeManager();
    ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
        (void)manager.getOrCreate(mps_rt::LibraryKey::Named(""));
    });
}

TYPED_TEST(MpsLibraryManagerTypedTest, ReleaseRejectsStaleId) {
    auto& manager = this->manager();
    this->initializeManager();
    const auto maybe_name = this->libraryNameFromEnv();
    if (!maybe_name.has_value()) {
        GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_LIBRARY_NAME " to a valid library name to run";
    }
    const auto key = mps_rt::LibraryKey::Named(*maybe_name);

    backend::mps::MPSLibrary_t handle = nullptr;
    if constexpr (TypeParam::is_mock) {
        handle = makeLibrary(0x650);
        this->adapter().expectCreateLibraries({{*maybe_name, handle}});
        this->adapter().expectDestroyLibraries({handle});
    }

    const auto id = manager.getOrCreate(key);
    manager.release(id);
    ExpectError(diag_error::OrteafErrc::InvalidState, [&] { manager.release(id); });
}

TYPED_TEST(MpsLibraryManagerTypedTest, GetLibraryRejectsInvalidId) {
    auto& manager = this->manager();
    this->initializeManager();
    ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
        (void)manager.getLibrary(base::LibraryId{std::numeric_limits<std::uint32_t>::max()});
    });
}

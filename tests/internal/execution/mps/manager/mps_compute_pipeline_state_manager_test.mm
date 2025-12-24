#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <limits>
#include <optional>
#include <string>
#include <system_error>

#include <orteaf/internal/diagnostics/error/error.h>
#include <orteaf/internal/execution/mps/manager/mps_compute_pipeline_state_manager.h>
#include <tests/internal/execution/mps/manager/testing/backend_ops_provider.h>
#include <tests/internal/execution/mps/manager/testing/manager_test_fixture.h>
#include <tests/internal/testing/error_assert.h>

namespace diag_error = orteaf::internal::diagnostics::error;
namespace mps_rt = orteaf::internal::execution::mps::manager;
namespace mps_wrapper = orteaf::internal::execution::mps::platform::wrapper;
namespace testing_mps = orteaf::tests::execution::mps::testing;
using orteaf::tests::ExpectError;

#define ORTEAF_MPS_ENV_LIBRARY_NAME "ORTEAF_EXPECT_MPS_LIBRARY_NAME"
#define ORTEAF_MPS_ENV_FUNCTION_NAME "ORTEAF_EXPECT_MPS_FUNCTION_NAME"

namespace {

mps_wrapper::MpsLibrary_t makeLibrary(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MpsLibrary_t>(value);
}

mps_wrapper::MpsFunction_t makeFunction(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MpsFunction_t>(value);
}

mps_wrapper::MpsComputePipelineState_t makePipeline(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MpsComputePipelineState_t>(value);
}

template <class Provider>
class MpsComputePipelineStateManagerTypedTest
    : public testing_mps::RuntimeManagerFixture<
          Provider, mps_rt::MpsComputePipelineStateManager> {
protected:
  using Base = testing_mps::RuntimeManagerFixture<
      Provider, mps_rt::MpsComputePipelineStateManager>;

  mps_rt::MpsComputePipelineStateManager &manager() { return Base::manager(); }

  using PipelineLease =
      typename mps_rt::MpsComputePipelineStateManager::PipelineLease;

  auto &adapter() { return Base::adapter(); }

  void TearDown() override {
    manager().shutdown();
    if (library_ != nullptr) {
      if constexpr (Provider::is_mock) {
        adapter().expectDestroyLibraries({library_});
      }
      this->getOps()->destroyLibrary(library_);
      library_ = nullptr;
    }
    Base::TearDown();
  }

  bool initializeManager(std::size_t capacity = 0) {
    const auto device = adapter().device();
    if (auto library = ensureLibrary()) {
      mps_rt::MpsComputePipelineStateManager::Config config{};
      config.device = device;
      config.library = *library;
      config.ops = this->getOps();
      config.payload_capacity = capacity;
      config.control_block_capacity = capacity;
      manager().configure(config);
      return true;
    }
    return false;
  }

  std::optional<std::string> libraryNameFromEnv() const {
    if constexpr (Provider::is_mock) {
      return std::string{"MockLibrary"};
    }
    const char *value = std::getenv(ORTEAF_MPS_ENV_LIBRARY_NAME);
    if (value == nullptr || *value == '\0') {
      return std::nullopt;
    }
    return std::string{value};
  }

  std::optional<std::string> functionNameFromEnv() const {
    if constexpr (Provider::is_mock) {
      return std::string{"MockFunction"};
    }
    const char *value = std::getenv(ORTEAF_MPS_ENV_FUNCTION_NAME);
    if (value == nullptr || *value == '\0') {
      return std::nullopt;
    }
    return std::string{value};
  }

  std::optional<mps_wrapper::MpsLibrary_t> ensureLibrary() {
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
    library_ = this->getOps()->createLibraryWithName(device, *maybe_name);
    return library_;
  }

  void skipMissingLibraryEnv() const {
    GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_LIBRARY_NAME
                    " to a valid library name to run";
  }

private:
  mps_wrapper::MpsLibrary_t library_{nullptr};
};

#if ORTEAF_ENABLE_MPS
using ProviderTypes = ::testing::Types<testing_mps::MockBackendOpsProvider,
                                       testing_mps::RealBackendOpsProvider>;
#else
using ProviderTypes = ::testing::Types<testing_mps::MockBackendOpsProvider>;
#endif

} // namespace

TYPED_TEST_SUITE(MpsComputePipelineStateManagerTypedTest, ProviderTypes);

// =============================================================================
// Configuration Tests
// =============================================================================

TYPED_TEST(MpsComputePipelineStateManagerTypedTest,
           GrowthChunkSizeCanBeAdjusted) {
  auto &manager = this->manager();

  const auto device = this->adapter().device();
  const auto maybe_library = this->ensureLibrary();
  if (!maybe_library.has_value()) {
    return;
  }

  // Assert: Default is 1
  EXPECT_EQ(manager.payloadGrowthChunkSizeForTest(), 1u);
  EXPECT_EQ(manager.controlBlockGrowthChunkSizeForTest(), 1u);

  mps_rt::MpsComputePipelineStateManager::Config config{};
  config.device = device;
  config.library = *maybe_library;
  config.ops = this->getOps();
  config.payload_growth_chunk_size = 5;
  config.control_block_growth_chunk_size = 6;

  // Act
  manager.configure(config);

  // Assert
  EXPECT_EQ(manager.payloadGrowthChunkSizeForTest(), 5u);
  EXPECT_EQ(manager.controlBlockGrowthChunkSizeForTest(), 6u);

  manager.shutdown();
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest,
           GrowthChunkSizeRejectsZero) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  const auto maybe_library = this->ensureLibrary();
  if (!maybe_library.has_value()) {
    return;
  }

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
    mps_rt::MpsComputePipelineStateManager::Config config{};
    config.device = device;
    config.library = *maybe_library;
    config.ops = this->getOps();
    config.payload_growth_chunk_size = 0;
    manager.configure(config);
  });
  ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
    mps_rt::MpsComputePipelineStateManager::Config config{};
    config.device = device;
    config.library = *maybe_library;
    config.ops = this->getOps();
    config.control_block_growth_chunk_size = 0;
    manager.configure(config);
  });
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest,
           GrowthChunkSizeControlsPoolExpansion) {
  if constexpr (!TypeParam::is_mock) {
    GTEST_SKIP() << "Mock-only test";
    return;
  }
  auto &manager = this->manager();

  // Arrange
  const auto device = this->adapter().device();
  const auto maybe_library = this->ensureLibrary();
  if (!maybe_library.has_value()) {
    return;
  }
  mps_rt::MpsComputePipelineStateManager::Config config{};
  config.device = device;
  config.library = *maybe_library;
  config.ops = this->getOps();
  config.payload_growth_chunk_size = 2;
  manager.configure(config);

  const auto key = mps_rt::FunctionKey::Named("ChunkedFunction");
  const auto function_handle = makeFunction(0x8801);
  const auto pipeline_handle = makePipeline(0x9901);
  this->adapter().expectCreateFunctions({{"ChunkedFunction", function_handle}});
  this->adapter().expectCreateComputePipelineStates(
      {{function_handle, pipeline_handle}});

  // Act
  auto lease = manager.acquire(key);

  // Assert: growOrAllocateSlot grows by growth_chunk_size_ (which is 2)
  EXPECT_EQ(manager.payloadPoolSizeForTest(), 2u);

  // Cleanup
  this->adapter().expectDestroyComputePipelineStates({pipeline_handle});
  this->adapter().expectDestroyFunctions({function_handle});
  lease.release();
  manager.shutdown();
}

// =============================================================================
// Initialization Tests
// =============================================================================

TYPED_TEST(MpsComputePipelineStateManagerTypedTest,
           AccessBeforeInitializationThrows) {
  auto &manager = this->manager();
  const auto key = mps_rt::FunctionKey::Named("Unused");

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.acquire(key); });
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest,
           InitializeRejectsNullDevice) {
  auto &manager = this->manager();
  const auto maybe_library = this->ensureLibrary();
  if (!maybe_library.has_value()) {
    return;
  }

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
    mps_rt::MpsComputePipelineStateManager::Config config{};
    config.device = nullptr;
    config.library = *maybe_library;
    config.ops = this->getOps();
    config.payload_capacity = 1;
    config.control_block_capacity = 1;
    manager.configure(config);
  });
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest,
           InitializeRejectsNullLibrary) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
    mps_rt::MpsComputePipelineStateManager::Config config{};
    config.device = device;
    config.library = nullptr;
    config.ops = this->getOps();
    config.payload_capacity = 1;
    config.control_block_capacity = 1;
    manager.configure(config);
  });
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest,
           InitializeRejectsCapacityAboveLimit) {
  auto &manager = this->manager();
  const auto device = this->adapter().device();
  const auto maybe_library = this->ensureLibrary();
  if (!maybe_library.has_value()) {
    return;
  }

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
    mps_rt::MpsComputePipelineStateManager::Config config{};
    config.device = device;
    config.library = *maybe_library;
    config.ops = this->getOps();
    config.payload_capacity = std::numeric_limits<std::size_t>::max();
    config.control_block_capacity =
        std::numeric_limits<std::size_t>::max();
    manager.configure(config);
  });
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest,
           InitializeWithZeroCapacityIsAllowed) {
  auto &manager = this->manager();
  if (!this->initializeManager(0)) {
    return;
  }

  // Assert
  EXPECT_EQ(manager.payloadPoolSizeForTest(), 0u);
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest, InitializeSetsCapacity) {
  auto &manager = this->manager();
  if (!this->initializeManager(2)) {
    return;
  }

  // Assert: Capacity mirrors configured size at initialization.
  EXPECT_EQ(manager.payloadPoolSizeForTest(), 2u);
}

// =============================================================================
// Acquire/Release Tests
// =============================================================================

TYPED_TEST(MpsComputePipelineStateManagerTypedTest,
           AcquireCreatesAndCachesPipeline) {
  auto &manager = this->manager();
  if (!this->initializeManager()) {
    return;
  }

  // Arrange
  const auto maybe_name = this->functionNameFromEnv();
  if (!maybe_name.has_value()) {
    GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_FUNCTION_NAME
                    " to a valid function name to run";
    return;
  }
  mps_wrapper::MpsFunction_t function_handle = nullptr;
  mps_wrapper::MpsComputePipelineState_t pipeline_handle = nullptr;
  if constexpr (TypeParam::is_mock) {
    function_handle = makeFunction(0x801);
    pipeline_handle = makePipeline(0x901);
    this->adapter().expectCreateFunctions({{*maybe_name, function_handle}});
    this->adapter().expectCreateComputePipelineStates(
        {{function_handle, pipeline_handle}});
  }

  const auto key = mps_rt::FunctionKey::Named(*maybe_name);

  // Act: Acquire twice with same key
  auto lease0 = manager.acquire(key);
  auto lease1 = manager.acquire(key);

  // Assert: Same handle (cached)
  EXPECT_EQ(lease0.payloadHandle(), lease1.payloadHandle());
  if constexpr (TypeParam::is_mock) {
    ASSERT_NE(lease0.payloadPtr(), nullptr);
    EXPECT_EQ(lease0.payloadPtr()->pipeline_state, pipeline_handle);
  } else {
    EXPECT_TRUE(lease0);
  }

  // Assert: State is initialized with valid resource
  EXPECT_TRUE(manager.payloadCreatedForTest(lease0.payloadHandle()));
  const auto *payload = manager.payloadForTest(lease0.payloadHandle());
  ASSERT_NE(payload, nullptr);
  EXPECT_NE(payload->pipeline_state, nullptr);
  EXPECT_NE(payload->function, nullptr);

  // Cleanup
  this->adapter().expectDestroyComputePipelineStates({pipeline_handle});
  this->adapter().expectDestroyFunctions({function_handle});
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest,
           ReleaseDestroysHandlesAndAllowsRecreation) {
  auto &manager = this->manager();
  if (!this->initializeManager()) {
    return;
  }

  // Arrange
  const auto maybe_name = this->functionNameFromEnv();
  if (!maybe_name.has_value()) {
    GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_FUNCTION_NAME
                    " to a valid function name to run";
    return;
  }

  mps_wrapper::MpsFunction_t first_function = nullptr;
  mps_wrapper::MpsComputePipelineState_t first_pipeline = nullptr;
  if constexpr (TypeParam::is_mock) {
    first_function = makeFunction(0x820);
    first_pipeline = makePipeline(0x920);
    this->adapter().expectCreateFunctions({{*maybe_name, first_function}});
    this->adapter().expectCreateComputePipelineStates(
        {{first_function, first_pipeline}});
    this->adapter().expectDestroyComputePipelineStates({first_pipeline});
    this->adapter().expectDestroyFunctions({first_function});
  }

  const auto key = mps_rt::FunctionKey::Named(*maybe_name);

  // Act
  auto lease = manager.acquire(key);
  const auto handle = lease.payloadHandle();
  lease.release();

  // Assert: State stays alive (cache pattern)
  EXPECT_TRUE(manager.payloadCreatedForTest(handle));

  // Act: Reacquire returns same cached resource
  auto reacquired = manager.acquire(key);
  EXPECT_EQ(reacquired.payloadHandle(), handle);
  if constexpr (TypeParam::is_mock) {
    ASSERT_NE(reacquired.payloadPtr(), nullptr);
    EXPECT_EQ(reacquired.payloadPtr()->pipeline_state, first_pipeline);
  } else {
    EXPECT_TRUE(reacquired);
  }

  // Cleanup
  reacquired.release();
  manager.shutdown();
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest,
           ManualReleaseInvalidatesLease) {
  auto &manager = this->manager();
  if (!this->initializeManager()) {
    return;
  }
  if constexpr (!TypeParam::is_mock) {
    GTEST_SKIP() << "Mock-only test";
    return;
  }

  // Arrange
  const auto maybe_name = this->functionNameFromEnv();
  if (!maybe_name.has_value()) {
    GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_FUNCTION_NAME
                    " to a valid function name to run";
    return;
  }

  const auto function_handle = makeFunction(0x8b0);
  const auto pipeline_handle = makePipeline(0x9b0);
  this->adapter().expectCreateFunctions({{*maybe_name, function_handle}});
  this->adapter().expectCreateComputePipelineStates(
      {{function_handle, pipeline_handle}});
  this->adapter().expectDestroyComputePipelineStates({pipeline_handle});
  this->adapter().expectDestroyFunctions({function_handle});

  // Act
  auto lease = manager.acquire(mps_rt::FunctionKey::Named(*maybe_name));
  const auto original_handle = lease.payloadHandle();
  manager.release(lease);

  // Assert
  EXPECT_FALSE(static_cast<bool>(lease));

  EXPECT_TRUE(manager.payloadCreatedForTest(original_handle));

  // Cleanup
  manager.shutdown();
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TYPED_TEST(MpsComputePipelineStateManagerTypedTest,
           FunctionCreationFailureIsReported) {
  auto &manager = this->manager();
  if (!this->initializeManager()) {
    return;
  }
  if constexpr (!TypeParam::is_mock) {
    GTEST_SKIP() << "Function creation failure scenario only applies to mocks";
    return;
  }

  // Arrange
  const auto maybe_name = this->functionNameFromEnv();
  if (!maybe_name.has_value()) {
    GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_FUNCTION_NAME
                    " to a valid function name to run";
    return;
  }
  this->adapter().expectCreateFunctions({{*maybe_name, nullptr}});

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidState, [&] {
    (void)manager.acquire(mps_rt::FunctionKey::Named(*maybe_name));
  });
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest,
           PipelineCreationFailureDestroysFunction) {
  auto &manager = this->manager();
  if (!this->initializeManager()) {
    return;
  }
  if constexpr (!TypeParam::is_mock) {
    GTEST_SKIP() << "Pipeline creation failure scenario only applies to mocks";
    return;
  }

  // Arrange
  const auto maybe_name = this->functionNameFromEnv();
  if (!maybe_name.has_value()) {
    GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_FUNCTION_NAME
                    " to a valid function name to run";
    return;
  }
  const auto function_handle = makeFunction(0x811);
  this->adapter().expectCreateFunctions({{*maybe_name, function_handle}});
  this->adapter().expectCreateComputePipelineStates(
      {{function_handle, nullptr}});
  this->adapter().expectDestroyFunctions({function_handle});

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidState, [&] {
    (void)manager.acquire(mps_rt::FunctionKey::Named(*maybe_name));
  });
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest, EmptyIdentifierIsRejected) {
  auto &manager = this->manager();
  if (!this->initializeManager()) {
    return;
  }

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidArgument,
              [&] { (void)manager.acquire(mps_rt::FunctionKey::Named("")); });
}

TYPED_TEST(MpsComputePipelineStateManagerTypedTest, ReleaseIgnoresStaleHandle) {
  auto &manager = this->manager();
  if (!this->initializeManager()) {
    return;
  }

  // Arrange
  const auto maybe_name = this->functionNameFromEnv();
  if (!maybe_name.has_value()) {
    GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_FUNCTION_NAME
                    " to a valid function name to run";
    return;
  }
  mps_wrapper::MpsFunction_t function_handle = nullptr;
  mps_wrapper::MpsComputePipelineState_t pipeline_handle = nullptr;
  if constexpr (TypeParam::is_mock) {
    function_handle = makeFunction(0x830);
    pipeline_handle = makePipeline(0x930);
    this->adapter().expectCreateFunctions({{*maybe_name, function_handle}});
    this->adapter().expectCreateComputePipelineStates(
        {{function_handle, pipeline_handle}});
    this->adapter().expectDestroyComputePipelineStates({pipeline_handle});
    this->adapter().expectDestroyFunctions({function_handle});
  }

  const auto key = mps_rt::FunctionKey::Named(*maybe_name);

  // Act & Assert: Second release is silently ignored
  auto lease = manager.acquire(key);
  lease.release();
  lease.release();
}

// =============================================================================
// Shutdown Tests
// =============================================================================

TYPED_TEST(MpsComputePipelineStateManagerTypedTest,
           ShutdownDestroysAllHandles) {
  auto &manager = this->manager();
  if (!this->initializeManager()) {
    return;
  }

  // Arrange
  const auto maybe_name = this->functionNameFromEnv();
  if (!maybe_name.has_value()) {
    GTEST_SKIP() << "Set " ORTEAF_MPS_ENV_FUNCTION_NAME
                    " to a valid function name to run";
    return;
  }
  mps_wrapper::MpsFunction_t function_handle = nullptr;
  mps_wrapper::MpsComputePipelineState_t pipeline_handle = nullptr;
  if constexpr (TypeParam::is_mock) {
    function_handle = makeFunction(0x840);
    pipeline_handle = makePipeline(0x940);
    this->adapter().expectCreateFunctions({{*maybe_name, function_handle}});
    this->adapter().expectCreateComputePipelineStates(
        {{function_handle, pipeline_handle}});
    this->adapter().expectDestroyComputePipelineStates({pipeline_handle});
    this->adapter().expectDestroyFunctions({function_handle});
  }
  const auto key = mps_rt::FunctionKey::Named(*maybe_name);

  // Act
  auto lease = manager.acquire(key);
  lease.release(); // Release before shutdown
  manager.shutdown();

  // Assert
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { (void)manager.acquire(key); });
}

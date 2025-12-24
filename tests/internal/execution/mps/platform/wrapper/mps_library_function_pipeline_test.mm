/**
 * @file mps_library_function_pipeline_test.mm
 * @brief Tests for MPS/Metal library, function, and pipeline state operations.
 */

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include "orteaf/internal/execution/mps/platform/wrapper/mps_compile_options.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_compute_pipeline_state.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_device.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_error.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_function.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_library.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_string.h"

#include "tests/internal/testing/error_assert.h"

#include <gtest/gtest.h>
#include <string>

namespace mps = orteaf::internal::execution::mps::platform::wrapper;

/**
 * @brief Test fixture for MPS library/function/pipeline tests.
 */
class MpsLibraryFunctionPipelineTest : public ::testing::Test {
protected:
  void SetUp() override {
    device_ = mps::getDevice();
    if (device_ == nullptr) {
      GTEST_SKIP() << "No Metal devices available";
    }
  }

  void TearDown() override {
    if (device_ != nullptr) {
      mps::deviceRelease(device_);
    }
  }

  mps::MpsDevice_t device_ = nullptr;
};

/**
 * @brief Test that destroy_library with nullptr is ignored.
 */
TEST_F(MpsLibraryFunctionPipelineTest, DestroyLibraryNullptrIsIgnored) {
  EXPECT_NO_THROW(mps::destroyLibrary(nullptr));
}

/**
 * @brief Test that destroy_function with nullptr is ignored.
 */
TEST_F(MpsLibraryFunctionPipelineTest, DestroyFunctionNullptrIsIgnored) {
  EXPECT_NO_THROW(mps::destroyFunction(nullptr));
}

/**
 * @brief Test that destroy_pipeline_state with nullptr is ignored.
 */
TEST_F(MpsLibraryFunctionPipelineTest,
       DestroyComputePipelineStateNullptrIsIgnored) {
  EXPECT_NO_THROW(mps::destroyComputePipelineState(nullptr));
}

/**
 * @brief Test that create_library_with_source handles invalid source.
 */
TEST_F(MpsLibraryFunctionPipelineTest, CreateLibraryWithSourceInvalidSource) {
  mps::MpsString_t source =
      mps::toNsString(std::string_view("invalid metal code"));
  mps::MpsCompileOptions_t options = mps::createCompileOptions();

  mps::MpsError_t error = nullptr;
  mps::MpsLibrary_t library =
      mps::createLibraryWithSource(device_, source, options, &error);

  // Should fail with invalid source
  EXPECT_EQ(library, nullptr);

  mps::destroyCompileOptions(options);
}

/**
 * @brief Test that create_library_with_data handles invalid data.
 */
TEST_F(MpsLibraryFunctionPipelineTest, CreateLibraryWithDataInvalidData) {
  const char invalid_data[] = "not a valid Metal library";
  mps::MpsError_t error = nullptr;
  mps::MpsLibrary_t library = mps::createLibraryWithData(
      device_, invalid_data, sizeof(invalid_data), &error);

  // Should fail with invalid data
  EXPECT_EQ(library, nullptr);
}

/**
 * @brief Test that create_function with nullptr library throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest, CreateFunctionNullptrLibraryThrows) {
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [] { mps::createFunction(nullptr, "kernel_name"); });
}

/**
 * @brief Test that create_function with empty name throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest, CreateFunctionEmptyNameThrows) {
  // Try to create a minimal valid library first
  mps::MpsError_t error = nullptr;
  mps::MpsString_t source =
      mps::toNsString(std::string_view("kernel void test() {}"));
  mps::MpsCompileOptions_t options = mps::createCompileOptions();
  mps::MpsLibrary_t library =
      mps::createLibraryWithSource(device_, source, options, &error);

  if (library != nullptr) {
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
        [&] { mps::createFunction(library, ""); });
    mps::destroyLibrary(library);
  }

  mps::destroyCompileOptions(options);
}

/**
 * @brief Test that create_pipeline_state with nullptr device throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest,
       CreateComputePipelineStateNullptrDevice) {
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [] { mps::createComputePipelineState(nullptr, nullptr); });
}

/**
 * @brief Test that create_pipeline_state with nullptr function throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest,
       CreateComputePipelineStateNullptrFunction) {
  mps::MpsError_t error = nullptr;
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [&] { mps::createComputePipelineState(device_, nullptr, &error); });
}

/**
 * @brief Test that compile options can be created and destroyed.
 */
TEST_F(MpsLibraryFunctionPipelineTest, CompileOptionsLifecycle) {
  mps::MpsCompileOptions_t options = mps::createCompileOptions();
  EXPECT_NE(options, nullptr);

  EXPECT_NO_THROW(mps::destroyCompileOptions(options));
}

/**
 * @brief Test that destroy_compile_options with nullptr is ignored.
 */
TEST_F(MpsLibraryFunctionPipelineTest, DestroyCompileOptionsNullptrIsIgnored) {
  EXPECT_NO_THROW(mps::destroyCompileOptions(nullptr));
}

/**
 * @brief Test that compile options can be configured.
 */
TEST_F(MpsLibraryFunctionPipelineTest, ConfigureCompileOptions) {
  mps::MpsCompileOptions_t options = mps::createCompileOptions();
  ASSERT_NE(options, nullptr);

  EXPECT_NO_THROW(mps::setCompileOptionsMathMode(options, true));
  EXPECT_NO_THROW(mps::setCompileOptionsPreserveInvariance(options, true));

  mps::destroyCompileOptions(options);
}

/**
 * @brief Test that set_compile_options_math_mode with nullptr throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest, SetCompileOptionsMathModeNullptrThrows) {
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [] { mps::setCompileOptionsMathMode(nullptr, true); });
}

/**
 * @brief Test that set_compile_options_preserve_invariance with nullptr throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest,
       SetCompileOptionsPreserveInvarianceNullptrThrows) {
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [] { mps::setCompileOptionsPreserveInvariance(nullptr, true); });
}

/**
 * @brief Test that set_compile_options_preprocessor_macros with nullptr throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest,
       SetCompileOptionsPreprocessorMacrosNullptrThrows) {
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [] { mps::setCompileOptionsPreprocessorMacros(nullptr, nullptr); });
}

/**
 * @brief Test that string conversion works.
 */
TEST_F(MpsLibraryFunctionPipelineTest, StringConversionWorks) {
  mps::MpsString_t str = mps::toNsString(std::string_view("test_string"));
  EXPECT_NE(str, nullptr);

  NSString *ns_str = (__bridge NSString *)str;
  EXPECT_NE(ns_str, nil);
  EXPECT_EQ([ns_str length], 11);
}

/**
 * @brief Test that library creation with nullptr device throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest, CreateLibraryNullptrDeviceThrows) {
  mps::MpsString_t name = mps::toNsString(std::string_view("default"));
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [&] { (void)mps::createLibrary(nullptr, name); });
}

/**
 * @brief Test that library creation with nullptr name throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest, CreateLibraryNullptrNameThrows) {
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
      [&] { (void)mps::createLibrary(device_, nullptr); });
}

/**
 * @brief Test that library creation with empty name throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest, CreateLibraryEmptyNameThrows) {
  mps::MpsString_t empty_name = mps::toNsString(std::string_view(""));
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
      [&] { (void)mps::createLibrary(device_, empty_name); });
}

/**
 * @brief Test that create_library_with_source with nullptr source throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest,
       CreateLibraryWithSourceNullptrSourceThrows) {
  mps::MpsCompileOptions_t options = mps::createCompileOptions();
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer, [&] {
        (void)mps::createLibraryWithSource(device_, nullptr, options, nullptr);
      });
  mps::destroyCompileOptions(options);
}

/**
 * @brief Test that create_library_with_source with empty source throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest,
       CreateLibraryWithSourceEmptySourceThrows) {
  mps::MpsString_t empty_source = mps::toNsString(std::string_view(""));
  mps::MpsCompileOptions_t options = mps::createCompileOptions();
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
      [&] {
        (void)mps::createLibraryWithSource(device_, empty_source, options,
                                           nullptr);
      });
  mps::destroyCompileOptions(options);
}

/**
 * @brief Test that create_library_with_data with nullptr data throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest, CreateLibraryWithDataNullptrDataThrows) {
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer, [&] {
        (void)mps::createLibraryWithData(device_, nullptr, 100, nullptr);
      });
}

/**
 * @brief Test that create_library_with_data with zero size throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest, CreateLibraryWithDataZeroSizeThrows) {
  const char data[] = "test data";
  ::orteaf::tests::ExpectError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
      [&] { (void)mps::createLibraryWithData(device_, data, 0, nullptr); });
}

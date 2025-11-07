/**
 * @file mps_library_function_pipeline_test.mm
 * @brief Tests for MPS/Metal library, function, and pipeline state operations.
 */

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

#include "orteaf/internal/backend/mps/mps_device.h"
#include "orteaf/internal/backend/mps/mps_library.h"
#include "orteaf/internal/backend/mps/mps_function.h"
#include "orteaf/internal/backend/mps/mps_pipeline_state.h"
#include "orteaf/internal/backend/mps/mps_string.h"
#include "orteaf/internal/backend/mps/mps_compile_options.h"
#include "orteaf/internal/backend/mps/mps_error.h"

#include <gtest/gtest.h>
#include <string>

namespace mps = orteaf::internal::backend::mps;

#ifdef ORTEAF_ENABLE_MPS

/**
 * @brief Test fixture for MPS library/function/pipeline tests.
 */
class MpsLibraryFunctionPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        device_ = mps::get_device();
        if (device_ == nullptr) {
            GTEST_SKIP() << "No Metal devices available";
        }
    }
    
    void TearDown() override {
        if (device_ != nullptr) {
            mps::device_release(device_);
        }
    }
    
    mps::MPSDevice_t device_ = nullptr;
};

/**
 * @brief Test that destroy_library with nullptr is ignored.
 */
TEST_F(MpsLibraryFunctionPipelineTest, DestroyLibraryNullptrIsIgnored) {
    EXPECT_NO_THROW(mps::destroy_library(nullptr));
}

/**
 * @brief Test that destroy_function with nullptr is ignored.
 */
TEST_F(MpsLibraryFunctionPipelineTest, DestroyFunctionNullptrIsIgnored) {
    EXPECT_NO_THROW(mps::destroy_function(nullptr));
}

/**
 * @brief Test that destroy_pipeline_state with nullptr is ignored.
 */
TEST_F(MpsLibraryFunctionPipelineTest, DestroyPipelineStateNullptrIsIgnored) {
    EXPECT_NO_THROW(mps::destroy_pipeline_state(nullptr));
}

/**
 * @brief Test that create_library_with_source handles invalid source.
 */
TEST_F(MpsLibraryFunctionPipelineTest, CreateLibraryWithSourceInvalidSource) {
    mps::MPSString_t source = mps::to_ns_string(std::string_view("invalid metal code"));
    mps::MPSCompileOptions_t options = mps::create_compile_options();
    
    mps::MPSError_t error = nullptr;
    mps::MPSLibrary_t library = mps::create_library_with_source(device_, source, options, &error);
    
    // Should fail with invalid source
    EXPECT_EQ(library, nullptr);
    
    mps::destroy_compile_options(options);
}

/**
 * @brief Test that create_library_with_data handles invalid data.
 */
TEST_F(MpsLibraryFunctionPipelineTest, CreateLibraryWithDataInvalidData) {
    const char invalid_data[] = "not a valid Metal library";
    mps::MPSError_t error = nullptr;
    mps::MPSLibrary_t library = mps::create_library_with_data(device_, invalid_data, sizeof(invalid_data), &error);
    
    // Should fail with invalid data
    EXPECT_EQ(library, nullptr);
}

/**
 * @brief Test that create_function with nullptr library throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest, CreateFunctionNullptrLibraryThrows) {
    EXPECT_THROW(mps::create_function(nullptr, "kernel_name"), std::system_error);
}

/**
 * @brief Test that create_function with empty name throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest, CreateFunctionEmptyNameThrows) {
    // Try to create a minimal valid library first
    mps::MPSError_t error = nullptr;
    mps::MPSString_t source = mps::to_ns_string(std::string_view("kernel void test() {}"));
    mps::MPSCompileOptions_t options = mps::create_compile_options();
    mps::MPSLibrary_t library = mps::create_library_with_source(device_, source, options, &error);
    
    if (library != nullptr) {
        EXPECT_THROW(mps::create_function(library, ""), std::system_error);
        mps::destroy_library(library);
    }
    
    mps::destroy_compile_options(options);
}

/**
 * @brief Test that create_pipeline_state with nullptr device throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest, CreatePipelineStateNullptrDevice) {
    EXPECT_THROW(mps::create_pipeline_state(nullptr, nullptr), std::system_error);
}

/**
 * @brief Test that create_pipeline_state with nullptr function throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest, CreatePipelineStateNullptrFunction) {
    mps::MPSError_t error = nullptr;
    EXPECT_THROW(mps::create_pipeline_state(device_, nullptr, &error), std::system_error);
}

/**
 * @brief Test that compile options can be created and destroyed.
 */
TEST_F(MpsLibraryFunctionPipelineTest, CompileOptionsLifecycle) {
    mps::MPSCompileOptions_t options = mps::create_compile_options();
    EXPECT_NE(options, nullptr);
    
    EXPECT_NO_THROW(mps::destroy_compile_options(options));
}

/**
 * @brief Test that destroy_compile_options with nullptr is ignored.
 */
TEST_F(MpsLibraryFunctionPipelineTest, DestroyCompileOptionsNullptrIsIgnored) {
    EXPECT_NO_THROW(mps::destroy_compile_options(nullptr));
}

/**
 * @brief Test that compile options can be configured.
 */
TEST_F(MpsLibraryFunctionPipelineTest, ConfigureCompileOptions) {
    mps::MPSCompileOptions_t options = mps::create_compile_options();
    ASSERT_NE(options, nullptr);
    
    EXPECT_NO_THROW(mps::set_compile_options_math_mode(options, true));
    EXPECT_NO_THROW(mps::set_compile_options_preserve_invariance(options, true));
    
    mps::destroy_compile_options(options);
}

/**
 * @brief Test that set_compile_options_math_mode with nullptr throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest, SetCompileOptionsMathModeNullptrThrows) {
    EXPECT_THROW(mps::set_compile_options_math_mode(nullptr, true), std::system_error);
}

/**
 * @brief Test that set_compile_options_preserve_invariance with nullptr throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest, SetCompileOptionsPreserveInvarianceNullptrThrows) {
    EXPECT_THROW(mps::set_compile_options_preserve_invariance(nullptr, true), std::system_error);
}

/**
 * @brief Test that set_compile_options_preprocessor_macros with nullptr throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest, SetCompileOptionsPreprocessorMacrosNullptrThrows) {
    EXPECT_THROW(mps::set_compile_options_preprocessor_macros(nullptr, nullptr), std::system_error);
}

/**
 * @brief Test that string conversion works.
 */
TEST_F(MpsLibraryFunctionPipelineTest, StringConversionWorks) {
    mps::MPSString_t str = mps::to_ns_string(std::string_view("test_string"));
    EXPECT_NE(str, nullptr);
    
    NSString* ns_str = (__bridge NSString*)str;
    EXPECT_NE(ns_str, nil);
    EXPECT_EQ([ns_str length], 11);
}

/**
 * @brief Test that library creation with nullptr device throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest, CreateLibraryNullptrDeviceThrows) {
    mps::MPSString_t name = mps::to_ns_string(std::string_view("default"));
    EXPECT_THROW((void)mps::create_library(nullptr, name), std::system_error);
}

/**
 * @brief Test that library creation with nullptr name throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest, CreateLibraryNullptrNameThrows) {
    EXPECT_THROW((void)mps::create_library(device_, nullptr), std::system_error);
}

/**
 * @brief Test that library creation with empty name throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest, CreateLibraryEmptyNameThrows) {
    mps::MPSString_t empty_name = mps::to_ns_string(std::string_view(""));
    EXPECT_THROW((void)mps::create_library(device_, empty_name), std::system_error);
}

/**
 * @brief Test that create_library_with_source with nullptr source throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest, CreateLibraryWithSourceNullptrSourceThrows) {
    mps::MPSCompileOptions_t options = mps::create_compile_options();
    EXPECT_THROW((void)mps::create_library_with_source(device_, nullptr, options, nullptr), std::system_error);
    mps::destroy_compile_options(options);
}

/**
 * @brief Test that create_library_with_source with empty source throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest, CreateLibraryWithSourceEmptySourceThrows) {
    mps::MPSString_t empty_source = mps::to_ns_string(std::string_view(""));
    mps::MPSCompileOptions_t options = mps::create_compile_options();
    EXPECT_THROW((void)mps::create_library_with_source(device_, empty_source, options, nullptr), std::system_error);
    mps::destroy_compile_options(options);
}

/**
 * @brief Test that create_library_with_data with nullptr data throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest, CreateLibraryWithDataNullptrDataThrows) {
    EXPECT_THROW((void)mps::create_library_with_data(device_, nullptr, 100, nullptr), std::system_error);
}

/**
 * @brief Test that create_library_with_data with zero size throws.
 */
TEST_F(MpsLibraryFunctionPipelineTest, CreateLibraryWithDataZeroSizeThrows) {
    const char data[] = "test data";
    EXPECT_THROW((void)mps::create_library_with_data(device_, data, 0, nullptr), std::system_error);
}

#else  // !ORTEAF_ENABLE_MPS

/**
 * @brief Test that library/function/pipeline functions return nullptr when MPS is disabled.
 */
TEST(MpsLibraryFunctionPipeline, DisabledReturnsNeutralValues) {
    EXPECT_EQ(mps::create_library(nullptr, nullptr), nullptr);
    EXPECT_EQ(mps::create_library_with_source(nullptr, nullptr, nullptr, nullptr), nullptr);
    EXPECT_EQ(mps::create_library_with_data(nullptr, nullptr, 0, nullptr), nullptr);
    EXPECT_EQ(mps::create_function(nullptr, "name"), nullptr);
    EXPECT_EQ(mps::create_pipeline_state(nullptr, nullptr), nullptr);
    
    EXPECT_NO_THROW(mps::destroy_library(nullptr));
    EXPECT_NO_THROW(mps::destroy_function(nullptr));
    EXPECT_NO_THROW(mps::destroy_pipeline_state(nullptr));
}

#endif  // ORTEAF_ENABLE_MPS

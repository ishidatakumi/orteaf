#include <gtest/gtest.h>

#include "orteaf/internal/backend/mps/wrapper/metal_kernel_embed_api.h"
#include "orteaf/internal/backend/mps/wrapper/mps_device.h"
#include "orteaf/internal/backend/mps/wrapper/mps_error.h"
#include "orteaf/internal/backend/mps/wrapper/mps_function.h"
#include "orteaf/internal/backend/mps/wrapper/mps_library.h"

namespace embed = orteaf::internal::backend::mps::metal_kernel_embed;
namespace mps = orteaf::internal::backend::mps;

namespace {

constexpr std::string_view kExistingLibrary = "embed_test_library";
constexpr std::string_view kExistingFunction = "orteaf_embed_test_identity";

bool hasDevice() {
    return mps::getDeviceCount() > 0;
}

}  // namespace

TEST(MetalKernelEmbedTest, EntriesExposeEmbeddedLibraryData) {
    auto entries = embed::libraries();
    ASSERT_FALSE(entries.empty());

    const auto blob = embed::findLibraryData(kExistingLibrary);
    EXPECT_NE(blob.data, nullptr);
    EXPECT_GT(blob.size, 0u);
    EXPECT_TRUE(embed::available(kExistingLibrary));
    EXPECT_FALSE(embed::available("nonexistent_metal_library"));

    // Ensure the registry span contains the expected entry.
    bool found = false;
    for (const auto& entry : entries) {
        if (std::string_view(entry.name) == kExistingLibrary) {
            found = true;
            EXPECT_NE(entry.begin, nullptr);
            EXPECT_NE(entry.end, nullptr);
            EXPECT_GT(entry.end, entry.begin);
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(MetalKernelEmbedTest, CreateEmbeddedLibraryFailsWithInvalidDevice) {
    mps::MPSError_t error = nullptr;
    mps::MPSLibrary_t lib = embed::createEmbeddedLibrary(nullptr, kExistingLibrary, &error);

    EXPECT_EQ(lib, nullptr);
    EXPECT_NE(error, nullptr);
    if (error) {
        mps::destroyError(error);
    }
}

TEST(MetalKernelEmbedTest, CreateEmbeddedLibraryFailsWithMissingLibrary) {
    if (!hasDevice()) {
        GTEST_SKIP() << "No MPS devices available";
    }
    mps::MPSError_t error = nullptr;
    mps::MPSDevice_t device = mps::getDevice(0);
    mps::MPSLibrary_t lib = embed::createEmbeddedLibrary(device, "nonexistent_metal_library", &error);

    EXPECT_EQ(lib, nullptr);
    EXPECT_NE(error, nullptr);
    if (error) {
        mps::destroyError(error);
    }
    mps::deviceRelease(device);
}

TEST(MetalKernelEmbedTest, CreateEmbeddedLibraryAndFunctionSucceeds) {
    if (!hasDevice()) {
        GTEST_SKIP() << "No MPS devices available";
    }
    mps::MPSError_t error = nullptr;
    mps::MPSDevice_t device = mps::getDevice(0);
    mps::MPSLibrary_t lib = embed::createEmbeddedLibrary(device, kExistingLibrary, &error);

    ASSERT_NE(lib, nullptr);
    EXPECT_EQ(error, nullptr);

    mps::MPSFunction_t fn = mps::createFunction(lib, kExistingFunction);
    EXPECT_NE(fn, nullptr);

    mps::destroyFunction(fn);
    mps::destroyLibrary(lib);
    mps::deviceRelease(device);
}

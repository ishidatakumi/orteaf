#include <gtest/gtest.h>

#include "orteaf/internal/backend/mps/wrapper/metal_kernel_embed_api.h"

using namespace orteaf::internal::backend::mps::metal_kernel_embed;

TEST(MetalKernelEmbedTest, EmbeddedLibraryIsAvailable) {
    constexpr std::string_view kLibraryName = "embed_test_library";
    MetallibBlob blob = findLibraryData(kLibraryName);

    EXPECT_NE(blob.data, nullptr);
    EXPECT_GT(blob.size, 0);
    EXPECT_TRUE(available(kLibraryName));
    EXPECT_FALSE(available("nonexistent_metal_library"));
}

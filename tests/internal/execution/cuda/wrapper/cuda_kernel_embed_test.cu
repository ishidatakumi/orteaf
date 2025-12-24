#include <gtest/gtest.h>

#include "orteaf/internal/execution/cuda/platform/wrapper/cuda_kernel_embed_api.h"
#include "tests/internal/testing/error_assert.h"

using namespace orteaf::internal::execution::cuda::platform::wrapper::kernel_embed;

namespace {

Blob find_any_available(std::string_view name) {
#if ORTEAF_EMBED_HAS_FATBIN
    Blob blob = findKernelData(name, CudaKernelFmt::Fatbin);
    if (blob.data) return blob;
#endif
#if ORTEAF_EMBED_HAS_CUBIN
    Blob blob = findKernelData(name, CudaKernelFmt::Cubin);
    if (blob.data) return blob;
#endif
#if ORTEAF_EMBED_HAS_PTX
    Blob blob = findKernelData(name, CudaKernelFmt::Ptx);
    if (blob.data) return blob;
#endif
    return {nullptr, 0};
}

}  // namespace

TEST(CudaKernelEmbedTest, CanRetrieveEmbeddedBlob) {
    constexpr std::string_view kKernelName = "embed_test_kernels";

    // Request a format that might not exist to validate fallback logic.
    Blob blob = findKernelData(kKernelName, CudaKernelFmt::Ptx);
    EXPECT_NE(blob.data, nullptr);
    EXPECT_GT(blob.size, 0);

    // Each advertised format should agree with available().
#if ORTEAF_EMBED_HAS_FATBIN
    EXPECT_TRUE(available(kKernelName, CudaKernelFmt::Fatbin));
#else
    EXPECT_FALSE(available(kKernelName, CudaKernelFmt::Fatbin));
#endif
#if ORTEAF_EMBED_HAS_CUBIN
    EXPECT_TRUE(available(kKernelName, CudaKernelFmt::Cubin));
#else
    EXPECT_FALSE(available(kKernelName, CudaKernelFmt::Cubin));
#endif
#if ORTEAF_EMBED_HAS_PTX
    EXPECT_TRUE(available(kKernelName, CudaKernelFmt::Ptx));
#else
    EXPECT_FALSE(available(kKernelName, CudaKernelFmt::Ptx));
#endif

    // Looking up a missing kernel should return an empty blob.
    Blob missing = findKernelData("nonexistent_kernel_for_embed_test", CudaKernelFmt::Fatbin);
    EXPECT_EQ(missing.data, nullptr);
    EXPECT_EQ(missing.size, 0);
}

TEST(CudaKernelEmbedTest, FindAnyAvailableReturnsBlobWhenFormatsPresent) {
    constexpr std::string_view kKernelName = "embed_test_kernels";
    Blob blob = find_any_available(kKernelName);

#if ORTEAF_EMBED_HAS_FATBIN || ORTEAF_EMBED_HAS_CUBIN || ORTEAF_EMBED_HAS_PTX
    ASSERT_NE(blob.data, nullptr);
    EXPECT_GT(blob.size, 0);
#else
    GTEST_SKIP() << "No CUDA kernel formats were generated.";
#endif
}

TEST(CudaKernelEmbedTest, EmptyKernelNameThrows) {
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
        [] { findKernelData(std::string_view{}, CudaKernelFmt::Fatbin); });
}

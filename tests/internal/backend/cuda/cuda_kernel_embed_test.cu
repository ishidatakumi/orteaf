#include <gtest/gtest.h>

#include "orteaf/internal/backend/cuda/cuda_kernel_embed_api.h"

using namespace orteaf::internal::backend::cuda::kernel_embed;

namespace {

Blob find_any_available(std::string_view name) {
#if ORTEAF_EMBED_HAS_FATBIN
    Blob blob = find_kernel_data(name, CudaKernelFmt::Fatbin);
    if (blob.data) return blob;
#endif
#if ORTEAF_EMBED_HAS_CUBIN
    Blob blob = find_kernel_data(name, CudaKernelFmt::Cubin);
    if (blob.data) return blob;
#endif
#if ORTEAF_EMBED_HAS_PTX
    Blob blob = find_kernel_data(name, CudaKernelFmt::Ptx);
    if (blob.data) return blob;
#endif
    return {nullptr, 0};
}

}  // namespace

#if ORTEAF_ENABLE_CUDA

TEST(CudaKernelEmbedTest, CanRetrieveEmbeddedBlob) {
    constexpr std::string_view kKernelName = "embed_test_kernels";

    // Request a format that might not exist to validate fallback logic.
    Blob blob = find_kernel_data(kKernelName, CudaKernelFmt::Ptx);
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
    Blob missing = find_kernel_data("nonexistent_kernel_for_embed_test", CudaKernelFmt::Fatbin);
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

#else  // !ORTEAF_ENABLE_CUDA

TEST(CudaKernelEmbedTest, SkippedWhenCudaDisabled) {
    GTEST_SKIP() << "CUDA backend disabled";
}

#endif  // ORTEAF_ENABLE_CUDA

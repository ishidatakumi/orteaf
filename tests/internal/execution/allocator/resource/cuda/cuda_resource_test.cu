#include "orteaf/internal/execution/allocator/resource/cuda/cuda_resource.h"

#if ORTEAF_ENABLE_CUDA

#include <gtest/gtest.h>

#include "tests/internal/testing/error_assert.h"

namespace orteaf::tests {
using orteaf::internal::execution::cuda::resource::CudaResource;
namespace diag_error = ::orteaf::internal::diagnostics::error;

TEST(CudaResourceTest, AllocateZeroThrows) {
    ExpectError(diag_error::OrteafErrc::InvalidParameter, [] {
        CudaResource::allocate(0, 256);
    });
}

TEST(CudaResourceTest, DeallocateEmptyIsNoOp) {
    EXPECT_NO_THROW(CudaResource::deallocate({}, 0, 0));
}

}  // namespace orteaf::tests

#endif  // ORTEAF_ENABLE_CUDA

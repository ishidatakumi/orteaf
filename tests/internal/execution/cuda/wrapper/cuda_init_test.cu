/**
 * @file cuda_init_test.cpp
 * @brief Tests for CUDA Driver API initialization.
 */

#include "orteaf/internal/execution/cuda/platform/wrapper/cuda_init.h"

#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace cuda = orteaf::internal::execution::cuda::platform::wrapper;

/**
 * @brief Test that CUDA initialization succeeds.
 */
TEST(CudaInit, InitializeSucceeds) {
    EXPECT_NO_THROW(cuda::cudaInit());
}

/**
 * @brief Test that CUDA initialization is idempotent (can be called multiple times).
 */
TEST(CudaInit, InitializeIsIdempotent) {
    cuda::cudaInit();
    EXPECT_NO_THROW(cuda::cudaInit());
    EXPECT_NO_THROW(cuda::cudaInit());
}

/**
 * @brief Test that CUDA initialization is thread-safe.
 */
TEST(CudaInit, InitializeIsThreadSafe) {
    constexpr int num_threads = 4;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([]() {
            EXPECT_NO_THROW(cuda::cudaInit());
            EXPECT_NO_THROW(cuda::cudaInit());
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // After all threads finish, initialization should still work
    EXPECT_NO_THROW(cuda::cudaInit());
}

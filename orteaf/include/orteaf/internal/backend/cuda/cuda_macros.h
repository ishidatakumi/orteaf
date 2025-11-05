#pragma once

// Handle cases where CUDA_AVAILABLE is defined but set to OFF/0
#if defined(CUDA_AVAILABLE) && !(CUDA_AVAILABLE)
#undef CUDA_AVAILABLE
#endif

#ifdef CUDA_AVAILABLE
#include <cuda_runtime.h>
#include <cublas_v2.h>
#include <stdexcept>
#include <string>
#endif

// ----------------------------------------------------------
// CUDA error check macro
// ----------------------------------------------------------
#ifdef CUDA_AVAILABLE
#define CUDA_CHECK(call)                                                                     \
    do {                                                                                     \
        cudaError_t err = call;                                                              \
        if (err != cudaSuccess) {                                                            \
            throw std::runtime_error(std::string("CUDA error: ") + cudaGetErrorString(err)); \
        }                                                                                    \
    } while (0)
#else
#define CUDA_CHECK(call) \
    do {                 \
        (void)(call);    \
    } while (0)
#endif

// ----------------------------------------------------------
// cuBLAS error check macro
// ----------------------------------------------------------
#ifdef CUDA_AVAILABLE
#define CUBLAS_CHECK(call)                                                                       \
    do {                                                                                         \
        cublasStatus_t status = call;                                                            \
        if (status != CUBLAS_STATUS_SUCCESS) {                                                   \
            throw std::runtime_error(std::string("cuBLAS error: ") + std::to_string(status));   \
        }                                                                                        \
    } while (0)
#else
#define CUBLAS_CHECK(call) \
    do {                   \
        (void)(call);      \
    } while (0)
#endif
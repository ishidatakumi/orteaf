#pragma once

#if ORTEAF_ENABLE_CUDA

#include <cstdint>

namespace orteaf::internal::runtime::cuda::platform::wrapper {

// Opaque handles and basic scalar aliases (centralized).
struct CudaDevice_st {
  int value{};

  constexpr CudaDevice_st() = default;
  constexpr explicit CudaDevice_st(int v) : value(v) {}
  constexpr operator int() const { return value; }
  friend constexpr bool operator==(CudaDevice_st lhs, CudaDevice_st rhs) {
    return lhs.value == rhs.value;
  }
  friend constexpr bool operator!=(CudaDevice_st lhs, CudaDevice_st rhs) {
    return !(lhs == rhs);
  }
  friend constexpr bool operator<(CudaDevice_st lhs, CudaDevice_st rhs) {
    return lhs.value < rhs.value;
  }
};
using CudaDevice_t = CudaDevice_st;
static_assert(sizeof(CudaDevice_t) == sizeof(int),
              "CudaDevice_t must remain int-sized (Driver API handle).");

struct CudaContext_st;
using CudaContext_t = CudaContext_st *;
static_assert(sizeof(CudaContext_t) == sizeof(void *),
              "CudaContext_t must be pointer-sized.");

struct CudaStream_st;
using CudaStream_t = CudaStream_st *;
static_assert(sizeof(CudaStream_t) == sizeof(void *),
              "CudaStream_t must be pointer-sized.");

using CudaDevicePtr_t = std::uint64_t;
static_assert(sizeof(CudaDevicePtr_t) == sizeof(std::uint64_t),
              "CudaDevicePtr_t must match 64-bit width.");

struct CudaEvent_st;
using CudaEvent_t = CudaEvent_st *;
static_assert(sizeof(CudaEvent_t) == sizeof(void *),
              "CudaEvent_t must be pointer-sized.");

struct CudaModule_st;
using CudaModule_t = CudaModule_st *;
static_assert(sizeof(CudaModule_t) == sizeof(void *),
              "CudaModule_t must be pointer-sized.");

struct CudaFunction_st;
using CudaFunction_t = CudaFunction_st *;
static_assert(sizeof(CudaFunction_t) == sizeof(void *),
              "CudaFunction_t must be pointer-sized.");

struct CudaGraph_st;
using CudaGraph_t = CudaGraph_st *;

struct CudaGraphExec_st;
using CudaGraphExec_t = CudaGraphExec_st *;

} // namespace orteaf::internal::runtime::cuda::platform::wrapper

#endif // ORTEAF_ENABLE_CUDA

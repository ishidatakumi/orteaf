#pragma once

#ifdef CUDA_AVAILABLE
#include <cuda.h>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace orteaf::internal::backend::cuda {

template <typename ObjcPtr>
static inline void* opaque_from_objc_noown(ObjcPtr p) noexcept {
    static_assert(std::is_pointer_v<ObjcPtr>, "ObjcPtr must be a pointer type");
    return reinterpret_cast<void*>(p);
}

template <typename ObjcPtr>
static inline ObjcPtr objc_from_opaque_noown(void* p) noexcept {
    static_assert(std::is_pointer_v<ObjcPtr>, "ObjcPtr must be a pointer type");
    return reinterpret_cast<ObjcPtr>(p);
}

template <typename Handle, typename ObjcPtr>
static inline Handle opaque_from_objc_noown(ObjcPtr p) noexcept {
    return reinterpret_cast<Handle>(reinterpret_cast<void*>(p));
}

template <typename Handle, typename ObjcPtr>
static inline ObjcPtr objc_from_opaque_noown(Handle p) noexcept {
    return reinterpret_cast<ObjcPtr>(reinterpret_cast<void*>(p));
}

static inline std::uint64_t opaque_from_cu_deviceptr(CUdeviceptr p) noexcept {
    return static_cast<std::uint64_t>(p);
}

static inline CUdeviceptr cu_deviceptr_from_opaque(std::uint64_t p) noexcept {
    return static_cast<CUdeviceptr>(p);
}

static inline int opaque_from_cu_device(CUdevice p) noexcept {
    return static_cast<int>(p);
}

static inline CUdevice cu_device_from_opaque(int p) noexcept {
    return static_cast<CUdevice>(p);
}

} // namespace orteaf::internal::backend::cuda

#endif
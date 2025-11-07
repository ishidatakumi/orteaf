/**
 * @file cuda_check.h
 * @brief Utilities to validate CUDA Runtime/Driver API results.
 *
 * This header maps CUDA return values to `OrteafError` (thrown as
 * `std::system_error`) with detailed messages. Prefer using the macros.
 *
 * - Runtime API: `cuda_check`, `cuda_check_last`, `cuda_check_sync`
 * - Driver API:  `cu_driver_check`, `try_driver_call`
 * - Macros:      `CUDA_CHECK`, `CUDA_CHECK_LAST`, `CUDA_CHECK_SYNC`, `CU_CHECK`
 *
 * When CUDA is disabled, functions and macros are no-ops.
 */
#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "orteaf/internal/diagnostics/error/error.h"

#if ORTEAF_ENABLE_CUDA
  #include <cuda_runtime.h>
  #include <cuda.h>
#endif

namespace orteaf::internal::backend::cuda {

#if ORTEAF_ENABLE_CUDA

/**
 * @brief Map a CUDA Runtime error to `OrteafErrc`.
 * @param err CUDA Runtime error code
 * @return Corresponding `OrteafErrc`
 */
inline orteaf::internal::diagnostics::error::OrteafErrc map_runtime_errc(cudaError_t err) {
    using orteaf::internal::diagnostics::error::OrteafErrc;
    switch (err) {
        case cudaSuccess:
            return OrteafErrc::Success;
        
        // Memory errors
        case cudaErrorMemoryAllocation:
            return OrteafErrc::OutOfMemory;
        
        // Invalid parameters, handles, descriptors, transfer directions, symbols, etc. (low-level parameter violations)
        case cudaErrorInvalidValue:
        case cudaErrorInvalidConfiguration:
        case cudaErrorInvalidPitchValue:
        case cudaErrorInvalidSymbol:
        case cudaErrorInvalidDevicePointer:
        case cudaErrorInvalidTexture:
        case cudaErrorInvalidTextureBinding:
        case cudaErrorInvalidChannelDescriptor:
        case cudaErrorInvalidMemcpyDirection:
        case cudaErrorInvalidResourceHandle:
        case cudaErrorInvalidFilterSetting:
        case cudaErrorInvalidNormSetting:
        case cudaErrorInvalidAddressSpace:
#if defined(cudaErrorInvalidHandle)
        case cudaErrorInvalidHandle:
#endif
#if defined(cudaErrorNotFound)
        case cudaErrorNotFound:
#endif
        case cudaErrorFileNotFound:
        case cudaErrorIllegalAddress:        // Invalid address reference from kernel
            return OrteafErrc::InvalidParameter;
        
        // Out of range (device ID, etc.)
        case cudaErrorInvalidDevice:
            return OrteafErrc::OutOfRange;
        
        // Invalid state (initialization order, already enabled/disabled, destroyed, etc.)
        case cudaErrorSetOnActiveProcess:
        case cudaErrorDeviceUninitialized:
        case cudaErrorPeerAccessAlreadyEnabled:
        case cudaErrorPeerAccessNotEnabled:
#if defined(cudaErrorPrimaryContextActive)
        case cudaErrorPrimaryContextActive:
#endif
        case cudaErrorContextIsDestroyed:
            return OrteafErrc::InvalidState;
        
        // Backend unavailable (environment/driver/initialization)
        case cudaErrorInitializationError:
        case cudaErrorNoDevice:
        case cudaErrorInsufficientDriver:
        case cudaErrorStartupFailure:
#if defined(cudaErrorDeviceUnavailable)
        case cudaErrorDeviceUnavailable:
#endif
            return OrteafErrc::BackendUnavailable;
        
        // Asynchronous operation not yet completed (not a failure, but "not ready yet")
        case cudaErrorNotReady:
            return OrteafErrc::NotReady;
        
        // Timeout
        case cudaErrorLaunchTimeout:
            return OrteafErrc::Timeout;
        
        // Device lost / hardware failure
#if defined(cudaErrorDeviceLost)
        case cudaErrorDeviceLost:
#endif
        case cudaErrorHardwareStackError:
            return OrteafErrc::DeviceLost;
        
        // Resource contention / busy
        case cudaErrorLaunchOutOfResources:
        case cudaErrorDeviceAlreadyInUse:
            return OrteafErrc::ResourceBusy;
        
        // Permission denied
        case cudaErrorNotPermitted:
            return OrteafErrc::PermissionDenied;
        
        // Unsupported / incompatible
        case cudaErrorInvalidDeviceFunction:
        case cudaErrorUnsupportedLimit:
        case cudaErrorPeerAccessUnsupported:
        case cudaErrorNotSupported:
        case cudaErrorLaunchIncompatibleTexturing:
            return OrteafErrc::Unsupported;
        
        // Compilation / load failures (PTX/code generation/loading)
        case cudaErrorInvalidPtx:
        case cudaErrorNoKernelImageForDevice:
        case cudaErrorJitCompilerNotFound:
        case cudaErrorIllegalInstruction:
        case cudaErrorInvalidPc:
        case cudaErrorInvalidSource:
        case cudaErrorSharedObjectSymbolNotFound:
        case cudaErrorSharedObjectInitFailed:
            return OrteafErrc::CompilationFailed;
        
        // Misalignment
        case cudaErrorMisalignedAddress:
            return OrteafErrc::Misaligned;
        
        // Known but miscellaneous failures (launch failure, OS-dependent, etc.) and unknown
        case cudaErrorLaunchFailure:
        case cudaErrorUnmapBufferObjectFailed:
        case cudaErrorOperatingSystem:
            return OrteafErrc::OperationFailed;
        
        case cudaErrorUnknown:
            return OrteafErrc::Unknown;
        
        default:
            // Unknown future error codes fall back to safe side
            return OrteafErrc::OperationFailed;
    }
}

/**
 * @brief Check a CUDA Runtime call result.
 * @param err CUDA Runtime return value
 * @param expr Stringified expression (auto-filled by macro)
 * @param file Caller file (auto-filled by macro)
 * @param line Caller line (auto-filled by macro)
 * @throws std::system_error Mapped from `OrteafErrc` on failure.
 *
 * On error, includes CUDA error name/code, expression, and file/line.
 * Prefer using `CUDA_CHECK(expr)`.
 */
inline void cuda_check(cudaError_t err,
                       const char* expr,
                       const char* file,
                       int line) {
    if (err != cudaSuccess) {
        using namespace orteaf::internal::diagnostics::error;
        std::string msg = "CUDA error: ";
        msg += cudaGetErrorString(err);
        msg += " (code ";
        msg += std::to_string(static_cast<int>(err));
        msg += ") while calling ";
        msg += expr;
        msg += " at ";
        msg += file;
        msg += ":";
        msg += std::to_string(line);
        throw_error(map_runtime_errc(err), std::move(msg));
    }
}

/**
 * @brief Check `cudaGetLastError()`.
 * @param file Caller file (auto-filled by macro)
 * @param line Caller line (auto-filled by macro)
 * @throws std::system_error If a recent CUDA Runtime error exists.
 *
 * Useful right after kernel launches. Prefer `CUDA_CHECK_LAST()`.
 */
inline void cuda_check_last(const char* file, int line) {
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        using namespace orteaf::internal::diagnostics::error;
        std::string msg = "CUDA error: ";
        msg += cudaGetErrorString(err);
        msg += " (code ";
        msg += std::to_string(static_cast<int>(err));
        msg += ") while calling cudaGetLastError() at ";
        msg += file;
        msg += ":";
        msg += std::to_string(line);
        throw_error(map_runtime_errc(err), std::move(msg));
    }
}

/**
 * @brief Debug-only stream synchronization and validation.
 * @param stream CUDA stream to synchronize
 * @param file Caller file (auto-filled by macro)
 * @param line Caller line (auto-filled by macro)
 *
 * When `ORTEAF_DEBUG_CUDA_SYNC` is defined, calls
 * `cudaStreamSynchronize(stream)` and validates with `cuda_check`.
 * Otherwise this is a no-op.
 */
inline void cuda_check_sync(cudaStream_t stream,
                            const char* file,
                            int line) {
#ifdef ORTEAF_DEBUG_CUDA_SYNC
    (void)file; (void)line;
    cuda_check(cudaStreamSynchronize(stream), "cudaStreamSynchronize(stream)", file, line);
#else
    (void)stream; (void)file; (void)line;
#endif
}

/**
 * @brief Map a CUDA Driver error to `OrteafErrc`.
 * @param err CUDA Driver error code (`CUresult`)
 * @return Corresponding `OrteafErrc`
 */
inline orteaf::internal::diagnostics::error::OrteafErrc map_driver_errc(CUresult err) {
    using orteaf::internal::diagnostics::error::OrteafErrc;
    switch (err) {
        case CUDA_SUCCESS:
            return OrteafErrc::Success;
        
        // Backend unavailable (environment/driver/initialization)
        case CUDA_ERROR_DEINITIALIZED:
        case CUDA_ERROR_NOT_INITIALIZED:
        case CUDA_ERROR_NO_DEVICE:
        case CUDA_ERROR_DEVICE_UNAVAILABLE:
            return OrteafErrc::BackendUnavailable;
        
        // Memory errors
        case CUDA_ERROR_OUT_OF_MEMORY:
            return OrteafErrc::OutOfMemory;
        
        // Invalid parameters, handles, descriptors, etc. (low-level parameter violations)
        case CUDA_ERROR_INVALID_VALUE:
        case CUDA_ERROR_INVALID_HANDLE:
        case CUDA_ERROR_NOT_FOUND:
        case CUDA_ERROR_ILLEGAL_ADDRESS:
            return OrteafErrc::InvalidParameter;
        
        // Out of range (device ID, etc.)
        case CUDA_ERROR_INVALID_DEVICE:
            return OrteafErrc::OutOfRange;
        
        // Invalid state (initialization order, already enabled/disabled, destroyed, etc.)
        case CUDA_ERROR_INVALID_CONTEXT:
        case CUDA_ERROR_PEER_ACCESS_ALREADY_ENABLED:
        case CUDA_ERROR_PEER_ACCESS_NOT_ENABLED:
        case CUDA_ERROR_PRIMARY_CONTEXT_ACTIVE:
        case CUDA_ERROR_CONTEXT_IS_DESTROYED:
            return OrteafErrc::InvalidState;
        
        // Asynchronous operation not yet completed (not a failure, but "not ready yet")
        case CUDA_ERROR_NOT_READY:
            return OrteafErrc::NotReady;
        
        // Timeout
        case CUDA_ERROR_LAUNCH_TIMEOUT:
            return OrteafErrc::Timeout;
        
        // Device lost / hardware failure
#if defined(CUDA_ERROR_DEVICE_LOST)
        case CUDA_ERROR_DEVICE_LOST:
#endif
            return OrteafErrc::DeviceLost;
        
        // Resource contention / busy
        case CUDA_ERROR_LAUNCH_OUT_OF_RESOURCES:
            return OrteafErrc::ResourceBusy;
        
        // Permission denied
        case CUDA_ERROR_DEVICE_NOT_LICENSED:
            return OrteafErrc::PermissionDenied;
        
        // Unsupported / incompatible
        case CUDA_ERROR_LAUNCH_INCOMPATIBLE_TEXTURING:
        case CUDA_ERROR_UNSUPPORTED_LIMIT:
            return OrteafErrc::Unsupported;
        
        // Compilation / load failures (PTX/code generation/loading)
        case CUDA_ERROR_SHARED_OBJECT_SYMBOL_NOT_FOUND:
        case CUDA_ERROR_SHARED_OBJECT_INIT_FAILED:
            return OrteafErrc::CompilationFailed;
        
        // Known but miscellaneous failures and unknown
        default:
            // Unknown future error codes fall back to safe side
            return OrteafErrc::OperationFailed;
    }
}

/**
 * @brief Check a CUDA Driver call result.
 * @param err Driver API return value
 * @param expr Stringified expression (auto-filled by macro)
 * @param file Caller file (auto-filled by macro)
 * @param line Caller line (auto-filled by macro)
 * @throws std::system_error Mapped from `OrteafErrc` on failure.
 *
 * When available, includes human-readable name/description via
 * `cuGetErrorName`/`cuGetErrorString`. Prefer `CU_CHECK(expr)`.
 */
inline void cu_driver_check(CUresult err,
                            const char* expr,
                            const char* file,
                            int line) {
    if (err != CUDA_SUCCESS) {
        using namespace orteaf::internal::diagnostics::error;
        const char* name = nullptr;
        const char* msg = nullptr;
        cuGetErrorName(err, &name);
        cuGetErrorString(err, &msg);
        std::string result = "CUDA driver error: ";
        if (name) {
            result += name;
            result += " (";
        } else {
            result += "unknown (";
        }
        result += std::to_string(static_cast<int>(err));
        result += ")";
        if (msg) {
            result += ": ";
            result += msg;
        }
        result += " while calling ";
        result += expr;
        result += " at ";
        result += file;
        result += ":";
        result += std::to_string(line);
        throw_error(map_driver_errc(err), std::move(result));
    }
}

/**
 * @brief Try a Driver API call and absorb a recoverable error.
 * @tparam Fn Callable type (e.g., lambda)
 * @param fn Function to execute; may throw driver-related `std::system_error`
 * @return `true` on success; `false` if `CUDA_ERROR_DEINITIALIZED` is detected.
 * @throws std::system_error Re-thrown for non-recoverable driver errors.
 *
 * Returns `false` for `CUDA_ERROR_DEINITIALIZED` to allow re-initialization;
 * other errors are propagated.
 */
template <typename Fn>
bool try_driver_call(Fn&& fn) {
    try {
        std::forward<Fn>(fn)();
        return true;
    } catch (const std::system_error& ex) {
        std::string_view what = ex.what();
        if (what.find("CUDA_ERROR_DEINITIALIZED") != std::string_view::npos) {
            return false;
        }
        throw;
    }
}

#else  // !ORTEAF_ENABLE_CUDA

/**
 * @brief No-op stub when CUDA is disabled (runtime check).
 */
inline void cuda_check(int, const char*, const char*, int) noexcept {}
/**
 * @brief No-op stub when CUDA is disabled (last error check).
 */
inline void cuda_check_last(const char*, int) noexcept {}
/**
 * @brief No-op stub when CUDA is disabled (debug sync).
 */
inline void cuda_check_sync(void*, const char*, int) noexcept {}
/**
 * @brief No-op stub when CUDA is disabled (driver check).
 */
inline void cu_driver_check(int, const char*, const char*, int) noexcept {}

template <typename Fn>
bool try_driver_call(Fn&& fn) {
    std::forward<Fn>(fn)();
    return true;
}

#endif // ORTEAF_ENABLE_CUDA

} // namespace orteaf::internal::backend::cuda

#if ORTEAF_ENABLE_CUDA
  /**
   * @def CUDA_CHECK(expr)
   * @brief Validate a CUDA Runtime API result and throw on failure.
   */
  #define CUDA_CHECK(expr)       ::orteaf::internal::backend::cuda::cuda_check((expr), #expr, __FILE__, __LINE__)
  /**
   * @def CUDA_CHECK_LAST()
   * @brief Validate the most recent CUDA Runtime error state.
   */
  #define CUDA_CHECK_LAST()      ::orteaf::internal::backend::cuda::cuda_check_last(__FILE__, __LINE__)
  /**
   * @def CUDA_CHECK_SYNC(s)
   * @brief Synchronize a stream and validate only when `ORTEAF_DEBUG_CUDA_SYNC` is defined.
   */
  #define CUDA_CHECK_SYNC(s)     ::orteaf::internal::backend::cuda::cuda_check_sync((s), __FILE__, __LINE__)
  /**
   * @def CU_CHECK(expr)
   * @brief Validate a CUDA Driver API result and throw on failure.
   */
  #define CU_CHECK(expr)         ::orteaf::internal::backend::cuda::cu_driver_check((expr), #expr, __FILE__, __LINE__)
#else
  /** @def CUDA_CHECK(expr)  @brief No-op when CUDA is disabled. */
  #define CUDA_CHECK(expr)       (void)(expr)
  /** @def CUDA_CHECK_LAST() @brief No-op when CUDA is disabled. */
  #define CUDA_CHECK_LAST()      ((void)0)
  /** @def CUDA_CHECK_SYNC(s) @brief No-op when CUDA is disabled. */
  #define CUDA_CHECK_SYNC(s)     ((void)0)
  /** @def CU_CHECK(expr)     @brief No-op when CUDA is disabled. */
  #define CU_CHECK(expr)         ((void)0)
#endif

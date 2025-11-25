#pragma once

/**
 * @file log_types.h
 * @brief ログ関連の型定義と constexpr ヘルパー関数。
 */

#include "orteaf/internal/diagnostics/log/log_config.h"

namespace orteaf::internal::diagnostics::log {

/**
 * @brief Log severity levels.
 *
 * Log levels are ordered from most verbose (Trace) to least verbose (Off).
 * Higher numeric values indicate more severe or important messages.
 */
enum class LogLevel : int {
    Trace = ORTEAF_LOG_LEVEL_TRACE_VAL,       ///< Most verbose level for detailed tracing information.
    Debug = ORTEAF_LOG_LEVEL_DEBUG_VAL,       ///< Debug information useful for development.
    Info = ORTEAF_LOG_LEVEL_INFO_VAL,         ///< Informational messages about normal operations.
    Warn = ORTEAF_LOG_LEVEL_WARN_VAL,         ///< Warning messages for potentially problematic situations.
    Error = ORTEAF_LOG_LEVEL_ERROR_VAL,       ///< Error messages for error conditions.
    Critical = ORTEAF_LOG_LEVEL_CRITICAL_VAL, ///< Critical error messages requiring immediate attention.
    Off = ORTEAF_LOG_LEVEL_OFF_VAL            ///< Disables all logging output.
};

/**
 * @brief Log message categories.
 *
 * Categories allow filtering and separate threshold configuration for different
 * subsystems of the library.
 */
enum class LogCategory : int {
    Core,   ///< Core library functionality.
    Tensor, ///< Tensor operations and management.
    Cuda,   ///< CUDA-specific operations.
    Mps,    ///< Metal Performance Shaders (MPS) operations.
    Io      ///< Input/output operations.
};

// ============================================================================
// コンパイル時ログレベル閾値
// ============================================================================

namespace detail {

/**
 * @brief Global log level threshold.
 *
 * Default threshold used when category-specific thresholds are not set.
 * Controlled by the `ORTEAF_LOG_LEVEL_GLOBAL_VALUE` compile-time macro.
 * If not defined, defaults to `LogLevel::Off`.
 */
inline constexpr int kLogLevelGlobal =
#ifdef ORTEAF_LOG_LEVEL_GLOBAL_VALUE
    ORTEAF_LOG_LEVEL_GLOBAL_VALUE;
#else
    static_cast<int>(LogLevel::Off);
#endif

/**
 * @brief Core category log level threshold.
 */
inline constexpr int kLogLevelCore =
#ifdef ORTEAF_LOG_LEVEL_CORE_VALUE
    ORTEAF_LOG_LEVEL_CORE_VALUE;
#else
    kLogLevelGlobal;
#endif

/**
 * @brief Tensor category log level threshold.
 */
inline constexpr int kLogLevelTensor =
#ifdef ORTEAF_LOG_LEVEL_TENSOR_VALUE
    ORTEAF_LOG_LEVEL_TENSOR_VALUE;
#else
    kLogLevelGlobal;
#endif

/**
 * @brief CUDA category log level threshold.
 */
inline constexpr int kLogLevelCuda =
#ifdef ORTEAF_LOG_LEVEL_CUDA_VALUE
    ORTEAF_LOG_LEVEL_CUDA_VALUE;
#else
    kLogLevelGlobal;
#endif

/**
 * @brief MPS category log level threshold.
 */
inline constexpr int kLogLevelMps =
#ifdef ORTEAF_LOG_LEVEL_MPS_VALUE
    ORTEAF_LOG_LEVEL_MPS_VALUE;
#else
    kLogLevelGlobal;
#endif

/**
 * @brief I/O category log level threshold.
 */
inline constexpr int kLogLevelIo =
#ifdef ORTEAF_LOG_LEVEL_IO_VALUE
    ORTEAF_LOG_LEVEL_IO_VALUE;
#else
    kLogLevelGlobal;
#endif
;

}  // namespace detail

// ============================================================================
// constexpr ヘルパー関数
// ============================================================================

/**
 * @brief Convert a LogLevel enum value to its integer representation.
 */
constexpr int levelToInt(LogLevel level) {
    return static_cast<int>(level);
}

/**
 * @brief Get the log level threshold for a specific category.
 */
template <LogCategory Category>
constexpr int categoryThreshold();

template <>
constexpr int categoryThreshold<LogCategory::Core>() {
    return detail::kLogLevelCore;
}

template <>
constexpr int categoryThreshold<LogCategory::Tensor>() {
    return detail::kLogLevelTensor;
}

template <>
constexpr int categoryThreshold<LogCategory::Cuda>() {
    return detail::kLogLevelCuda;
}

template <>
constexpr int categoryThreshold<LogCategory::Mps>() {
    return detail::kLogLevelMps;
}

template <>
constexpr int categoryThreshold<LogCategory::Io>() {
    return detail::kLogLevelIo;
}

/**
 * @brief Compile-time check whether a category permits a given level.
 */
template <LogCategory Category, LogLevel Level>
constexpr bool isLevelEnabled() {
    return levelToInt(Level) >= categoryThreshold<Category>();
}

// ============================================================================
// カテゴリ別 constexpr ヘルパー
// ============================================================================

/// @brief Core-category helper to check a specific level.
template <LogLevel Level>
constexpr bool coreLevelEnabled() {
    return isLevelEnabled<LogCategory::Core, Level>();
}

inline constexpr bool coreTraceEnabled() { return coreLevelEnabled<LogLevel::Trace>(); }
inline constexpr bool coreDebugEnabled() { return coreLevelEnabled<LogLevel::Debug>(); }
inline constexpr bool coreInfoEnabled()  { return coreLevelEnabled<LogLevel::Info>(); }

/// @brief Tensor-category helper to check a specific level.
template <LogLevel Level>
constexpr bool tensorLevelEnabled() {
    return isLevelEnabled<LogCategory::Tensor, Level>();
}

inline constexpr bool tensorTraceEnabled() { return tensorLevelEnabled<LogLevel::Trace>(); }
inline constexpr bool tensorDebugEnabled() { return tensorLevelEnabled<LogLevel::Debug>(); }
inline constexpr bool tensorInfoEnabled()  { return tensorLevelEnabled<LogLevel::Info>(); }

/// @brief CUDA-category helper to check a specific level.
template <LogLevel Level>
constexpr bool cudaLevelEnabled() {
    return isLevelEnabled<LogCategory::Cuda, Level>();
}

inline constexpr bool cudaTraceEnabled() { return cudaLevelEnabled<LogLevel::Trace>(); }
inline constexpr bool cudaDebugEnabled() { return cudaLevelEnabled<LogLevel::Debug>(); }
inline constexpr bool cudaInfoEnabled()  { return cudaLevelEnabled<LogLevel::Info>(); }

/// @brief MPS-category helper to check a specific level.
template <LogLevel Level>
constexpr bool mpsLevelEnabled() {
    return isLevelEnabled<LogCategory::Mps, Level>();
}

inline constexpr bool mpsTraceEnabled() { return mpsLevelEnabled<LogLevel::Trace>(); }
inline constexpr bool mpsDebugEnabled() { return mpsLevelEnabled<LogLevel::Debug>(); }
inline constexpr bool mpsInfoEnabled()  { return mpsLevelEnabled<LogLevel::Info>(); }

/// @brief IO-category helper to check a specific level.
template <LogLevel Level>
constexpr bool ioLevelEnabled() {
    return isLevelEnabled<LogCategory::Io, Level>();
}

inline constexpr bool ioTraceEnabled() { return ioLevelEnabled<LogLevel::Trace>(); }
inline constexpr bool ioDebugEnabled() { return ioLevelEnabled<LogLevel::Debug>(); }
inline constexpr bool ioInfoEnabled()  { return ioLevelEnabled<LogLevel::Info>(); }

// ============================================================================
// 文字列変換
// ============================================================================

/**
 * @brief Convert a LogLevel enum value to its string representation.
 */
constexpr const char* levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:    return "TRACE";
        case LogLevel::Debug:    return "DEBUG";
        case LogLevel::Info:     return "INFO";
        case LogLevel::Warn:     return "WARN";
        case LogLevel::Error:    return "ERROR";
        case LogLevel::Critical: return "CRITICAL";
        case LogLevel::Off:
        default:                 return "OFF";
    }
}

/**
 * @brief Convert a LogCategory enum value to its string representation.
 */
constexpr const char* categoryToString(LogCategory category) {
    switch (category) {
        case LogCategory::Core:   return "core";
        case LogCategory::Tensor: return "tensor";
        case LogCategory::Cuda:   return "cuda";
        case LogCategory::Mps:    return "mps";
        case LogCategory::Io:
        default:                  return "io";
    }
}

}  // namespace orteaf::internal::diagnostics::log

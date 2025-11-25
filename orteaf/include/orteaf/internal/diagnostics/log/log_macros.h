#pragma once

/**
 * @file log_macros.h
 * @brief ログ出力用マクロとアサーションマクロ。
 */

#include <string>

#include "orteaf/internal/diagnostics/log/log_sink.h"
#include "orteaf/internal/diagnostics/error/error_macros.h"

// ============================================================================
// 基本ログマクロ
// ============================================================================

/**
 * @def ORTEAF_LOG_INTERNAL(category, level, expr)
 * @brief Internal macro for lazy-evaluated logging.
 *
 * This macro is used internally by the public logging macros. It creates a lambda
 * that builds the log message only if the log level meets the category threshold.
 *
 * @param category Log category (e.g., `Core`, `Tensor`, `Cuda`).
 * @param level Log level (e.g., `Trace`, `Debug`, `Info`).
 * @param expr Expression that evaluates to the log message.
 */
#define ORTEAF_LOG_INTERNAL(category, level, expr)                                \
    ::orteaf::internal::diagnostics::log::detail::logLazy<                        \
        ::orteaf::internal::diagnostics::log::LogCategory::category,              \
        ::orteaf::internal::diagnostics::log::LogLevel::level>(                   \
        [&]() -> std::string { return std::string(expr); })

/**
 * @def ORTEAF_LOG_TRACE(category, expr)
 * @brief Log a message at TRACE level.
 */
#define ORTEAF_LOG_TRACE(category, expr) ORTEAF_LOG_INTERNAL(category, Trace, expr)

/**
 * @def ORTEAF_LOG_DEBUG(category, expr)
 * @brief Log a message at DEBUG level.
 */
#define ORTEAF_LOG_DEBUG(category, expr) ORTEAF_LOG_INTERNAL(category, Debug, expr)

/**
 * @def ORTEAF_LOG_INFO(category, expr)
 * @brief Log a message at INFO level.
 */
#define ORTEAF_LOG_INFO(category, expr) ORTEAF_LOG_INTERNAL(category, Info, expr)

/**
 * @def ORTEAF_LOG_WARN(category, expr)
 * @brief Log a message at WARN level.
 */
#define ORTEAF_LOG_WARN(category, expr) ORTEAF_LOG_INTERNAL(category, Warn, expr)

/**
 * @def ORTEAF_LOG_ERROR(category, expr)
 * @brief Log a message at ERROR level.
 */
#define ORTEAF_LOG_ERROR(category, expr) ORTEAF_LOG_INTERNAL(category, Error, expr)

/**
 * @def ORTEAF_LOG_CRITICAL(category, expr)
 * @brief Log a message at CRITICAL level.
 */
#define ORTEAF_LOG_CRITICAL(category, expr) ORTEAF_LOG_INTERNAL(category, Critical, expr)

// ============================================================================
// 条件付きログマクロ
// ============================================================================

/**
 * @def ORTEAF_LOG_INTERNAL_IF(category, level, condition, expr)
 * @brief Internal macro for conditional lazy-evaluated logging.
 *
 * The condition is only evaluated if logging is enabled for this category/level.
 */
#define ORTEAF_LOG_INTERNAL_IF(category, level, condition, expr)                  \
    ::orteaf::internal::diagnostics::log::detail::logLazyIf<                      \
        ::orteaf::internal::diagnostics::log::LogCategory::category,              \
        ::orteaf::internal::diagnostics::log::LogLevel::level>(                   \
        [&]() -> bool { return (condition); },                                    \
        [&]() -> std::string { return std::string(expr); })

/**
 * @def ORTEAF_LOG_TRACE_IF(category, condition, expr)
 * @brief Log a message at TRACE level if condition is true.
 */
#define ORTEAF_LOG_TRACE_IF(category, condition, expr) \
    ORTEAF_LOG_INTERNAL_IF(category, Trace, condition, expr)

/**
 * @def ORTEAF_LOG_DEBUG_IF(category, condition, expr)
 * @brief Log a message at DEBUG level if condition is true.
 */
#define ORTEAF_LOG_DEBUG_IF(category, condition, expr) \
    ORTEAF_LOG_INTERNAL_IF(category, Debug, condition, expr)

/**
 * @def ORTEAF_LOG_INFO_IF(category, condition, expr)
 * @brief Log a message at INFO level if condition is true.
 */
#define ORTEAF_LOG_INFO_IF(category, condition, expr) \
    ORTEAF_LOG_INTERNAL_IF(category, Info, condition, expr)

/**
 * @def ORTEAF_LOG_WARN_IF(category, condition, expr)
 * @brief Log a message at WARN level if condition is true.
 */
#define ORTEAF_LOG_WARN_IF(category, condition, expr) \
    ORTEAF_LOG_INTERNAL_IF(category, Warn, condition, expr)

/**
 * @def ORTEAF_LOG_ERROR_IF(category, condition, expr)
 * @brief Log a message at ERROR level if condition is true.
 */
#define ORTEAF_LOG_ERROR_IF(category, condition, expr) \
    ORTEAF_LOG_INTERNAL_IF(category, Error, condition, expr)

/**
 * @def ORTEAF_LOG_CRITICAL_IF(category, condition, expr)
 * @brief Log a message at CRITICAL level if condition is true.
 */
#define ORTEAF_LOG_CRITICAL_IF(category, condition, expr) \
    ORTEAF_LOG_INTERNAL_IF(category, Critical, condition, expr)

// ============================================================================
// デバッグ専用マクロ
// ============================================================================

/**
 * @def ORTEAF_DEBUG_THROW_IF(cond, code, msg)
 * @brief デバッグビルドでのみ条件チェックを行い、失敗時に例外を送出する。
 *
 * リリースビルドでは何も実行されない。
 *
 * @param cond 評価する条件式
 * @param code OrteafErrc のメンバ名
 * @param msg エラーメッセージ
 */

/**
 * @def ORTEAF_DEBUG_THROW_UNLESS(cond, code, msg)
 * @brief デバッグビルドでのみ条件チェックを行い、偽の場合に例外を送出する。
 *
 * リリースビルドでは何も実行されない。
 *
 * @param cond 評価する条件式
 * @param code OrteafErrc のメンバ名
 * @param msg エラーメッセージ
 */

#if ORTEAF_CORE_DEBUG_ENABLED
#define ORTEAF_DEBUG_THROW_IF(cond, code, msg) ORTEAF_THROW_IF(cond, code, msg)
#define ORTEAF_DEBUG_THROW_UNLESS(cond, code, msg) ORTEAF_THROW_UNLESS(cond, code, msg)
#else
#define ORTEAF_DEBUG_THROW_IF(cond, code, msg) ((void)0)
#define ORTEAF_DEBUG_THROW_UNLESS(cond, code, msg) ((void)0)
#endif

// ============================================================================
// アサーションマクロ
// ============================================================================

/**
 * @def ORTEAF_ASSERT(expr, message)
 * @brief Assertion macro with logging and error handling.
 *
 * If the expression evaluates to false, this macro:
 * 1. Logs a CRITICAL level message to the Core category.
 * 2. Calls fatalError() with an InvalidState error code.
 *
 * This macro is intended for runtime assertions that indicate an invalid program state.
 * The program will terminate after logging and error handling.
 *
 * @param expr Boolean expression to assert.
 * @param message String message describing the assertion failure.
 */
#define ORTEAF_ASSERT(expr, message)                                              \
    do {                                                                          \
        if (!(expr)) {                                                            \
            const std::string _orteaf_assert_message = std::string(message);      \
            ORTEAF_LOG_CRITICAL(Core, _orteaf_assert_message);                    \
            ::orteaf::internal::diagnostics::error::fatalError(                   \
                ::orteaf::internal::diagnostics::error::makeError(                \
                    ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState, \
                    _orteaf_assert_message));                                     \
        }                                                                         \
    } while (0)

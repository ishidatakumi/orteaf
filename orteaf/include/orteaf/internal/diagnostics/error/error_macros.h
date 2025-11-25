#pragma once

/**
 * @file error_macros.h
 * @brief エラーハンドリング用のヘルパーマクロ。
 *
 * throwError / fatalError の呼び出しを簡潔に記述するためのマクロを提供する。
 */

#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::diagnostics::error {

/// @brief 内部ヘルパー: namespace 修飾を省略して throwError を呼び出す
template <typename... Args>
[[noreturn]] inline void throwErrorHelper(OrteafErrc errc, Args&&... args) {
    throwError(errc, std::forward<Args>(args)...);
}

}  // namespace orteaf::internal::diagnostics::error

/**
 * @brief 指定したエラーコードとメッセージで例外を送出する。
 * @param code OrteafErrc のメンバ名（例: InvalidArgument）
 * @param msg エラーメッセージ（文字列リテラルまたは std::string）
 */
#define ORTEAF_THROW(code, msg) \
    ::orteaf::internal::diagnostics::error::throwErrorHelper( \
        ::orteaf::internal::diagnostics::error::OrteafErrc::code, msg)

/**
 * @brief 条件が真の場合に例外を送出する。
 * @param cond 評価する条件式
 * @param code OrteafErrc のメンバ名
 * @param msg エラーメッセージ
 */
#define ORTEAF_THROW_IF(cond, code, msg) \
    do { \
        if (cond) { \
            ORTEAF_THROW(code, msg); \
        } \
    } while (false)

/**
 * @brief 条件が偽の場合に例外を送出する。
 * @param cond 評価する条件式
 * @param code OrteafErrc のメンバ名
 * @param msg エラーメッセージ
 */
#define ORTEAF_THROW_UNLESS(cond, code, msg) \
    ORTEAF_THROW_IF(!(cond), code, msg)

/**
 * @brief ポインタが nullptr の場合に例外を送出する。
 * @param ptr 検査するポインタ
 * @param msg エラーメッセージ
 */
#define ORTEAF_THROW_IF_NULL(ptr, msg) \
    ORTEAF_THROW_IF((ptr) == nullptr, NullPointer, msg)


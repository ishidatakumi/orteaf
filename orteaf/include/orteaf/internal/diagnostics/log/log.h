#pragma once

/**
 * @file log.h
 * @brief ログ機能の統合ヘッダー。
 *
 * このヘッダーは後方互換性のため、すべてのログ関連ヘッダーを include する。
 * 必要に応じて個別のヘッダーを直接 include することも可能：
 *
 * - `log_config.h`: ログレベル定義、`ORTEAF_*_ENABLED` マクロ（依存なし）
 * - `log_types.h`: `LogLevel`, `LogCategory` enum、constexpr ヘルパー
 * - `log_sink.h`: `LogSink` 関連
 * - `log_macros.h`: `ORTEAF_LOG_*`, `ORTEAF_ASSERT` マクロ
 */

#include "orteaf/internal/diagnostics/log/log_config.h"
#include "orteaf/internal/diagnostics/log/log_types.h"
#include "orteaf/internal/diagnostics/log/log_sink.h"
#include "orteaf/internal/diagnostics/log/log_macros.h"

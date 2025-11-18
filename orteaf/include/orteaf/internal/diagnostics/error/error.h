#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

namespace orteaf::internal::diagnostics::error {

/**
 * @brief orteaf 固有のエラーコード。
 *
 * 成功(0)、未知エラー、引数不正、状態不正、バックエンド不可、メモリ不足、
 * 一般的な操作失敗、および低レイヤの実装パラメータ不備といった分類を保持する。
 */
enum class OrteafErrc {
    Success = 0,              ///< 成功
    Unknown = 1,              ///< 原因不明のエラー
    InvalidArgument = 2,      ///< 上位APIの引数不正（ユーザー入力：Tensor演算など）
    InvalidState = 3,         ///< オブジェクト状態が不正
    BackendUnavailable = 4,   ///< バックエンドが利用不可
    OutOfMemory = 5,          ///< メモリ確保に失敗
    OperationFailed = 6,      ///< その他の操作失敗
    
    // 低レイヤの実装パラメータ不備
    InvalidParameter = 7,     ///< 低レイヤのパラメータ不正（nullptr/サイズ/範囲/アライン等）
    NullPointer = 8,          ///< ポインタがnullptr
    OutOfRange = 9,           ///< 範囲外アクセス（device ID、index、dimension等）
    Misaligned = 10,          ///< アラインメント不一致
    NotReady = 11,            ///< 非同期処理が未完了
    Timeout = 12,             ///< APIがタイムアウト
    DeviceLost = 13,          ///< GPUデバイス喪失
    ResourceBusy = 14,        ///< リソース占有中
    PermissionDenied = 15,     ///< 権限不足
    Unsupported = 16,         ///< 機能未対応
    CompilationFailed = 17,    ///< コンパイル/ロード失敗
};

/// @brief std::error_code に適合する make_error_code 実装（前方宣言）。
std::error_code make_error_code(OrteafErrc errc);

/// @brief エラーコードを生成するヘルパ（前方宣言）。
std::error_code makeErrorCode(OrteafErrc errc);

}  // namespace orteaf::internal::diagnostics::error

namespace std {

// OrteafErrc を std::error_code として扱えるようにする。
template <>
struct is_error_code_enum<orteaf::internal::diagnostics::error::OrteafErrc> : true_type {};

}  // namespace std

namespace orteaf::internal::diagnostics::error {

/**
 * @brief ORTEAF 用の error_category。
 */
class OrteafErrorCategory : public std::error_category {
public:
    /// カテゴリ名。
    const char* name() const noexcept override;

    /// エラーコードに対応するメッセージ。
    std::string message(int condition) const override;
};

/// @brief エラーカテゴリのシングルトンを取得。
const std::error_category& orteafErrorCategory();

/// @brief エラーコードを生成するヘルパ。
std::error_code makeErrorCode(OrteafErrc errc);

/**
 * @brief ORTEAF 専用のエラー情報。
 *
 * `std::error_code`（= OrteafErrc + カテゴリ）と、任意の詳細メッセージを保持する。
 * 例外を投げる場合も、結果オブジェクトとして返す場合も、この構造体を経由する。
 */
class OrteafError {
public:
    OrteafError();
    OrteafError(std::error_code ec, std::string message = {});
    OrteafError(OrteafErrc errc, std::string message = {});

    /// エラー種別（OrteafErrc）を取得。
    OrteafErrc errc() const noexcept;

    /// 詳細メッセージ（空の場合あり）。
    std::string_view detail() const noexcept;

    /// カテゴリメッセージと詳細を結合した説明文字列を返す。
    std::string describe() const;

    /// 保持している std::error_code。
    const std::error_code& code() const noexcept;

    /// メッセージ文字列（mutable）。
    const std::string& message() const noexcept;

    /// エラーコードを設定（カテゴリ付き）。
    void setCode(std::error_code ec);

    /// エラーコードを ORTEAF カテゴリで設定。
    void setCode(OrteafErrc errc);

    /// 詳細メッセージを設定。
    void setMessage(std::string message);

private:
    std::error_code code_;
    std::string message_;
};

/// @brief エラーを生成するヘルパ。
OrteafError makeError(OrteafErrc errc, std::string message = {});
OrteafError makeError(std::error_code ec, std::string message = {});

/// @brief エラーを送出するヘルパ。内部で std::system_error を生成して投げる。
[[noreturn]] void throwError(const OrteafError& error);
[[noreturn]] void throwError(OrteafErrc errc, std::string message = {});
[[noreturn]] void throwError(std::error_code ec, std::string message = {});

/// @brief 致命的エラー。ログ出力後にプログラムを停止する。
[[noreturn]] void fatalError(const OrteafError& error);
[[noreturn]] void fatalError(OrteafErrc errc, std::string message = {});
[[noreturn]] void fatalError(std::error_code ec, std::string message = {});

/**
 * @brief Result 型の内部実装。
 */
namespace detail {

template <typename T>
class OrteafResultImpl {
public:
    static OrteafResultImpl success(T value);
    static OrteafResultImpl failure(OrteafError error);

    bool has_value() const noexcept;
    bool has_error() const noexcept;

    T& value() &;
    const T& value() const&;
    T&& value() &&;

    T value_or(T default_value) const&;

    OrteafError error() const;

private:
    template <typename V>
    OrteafResultImpl(std::in_place_index_t<0>, V&& value);
    OrteafResultImpl(std::in_place_index_t<1>, OrteafError error);

    std::optional<T> value_;
    std::optional<OrteafError> error_;
};

template <>
class OrteafResultImpl<void> {
public:
    static OrteafResultImpl success();
    static OrteafResultImpl failure(OrteafError error);

    bool has_value() const noexcept;
    bool has_error() const noexcept;

    void value() const;
    void value_or() const;

    OrteafError error() const;

private:
    OrteafResultImpl();
    bool has_value_{true};
    std::optional<OrteafError> error_{};
};

}  // namespace detail

/**
 * @brief ORTEAF 向けの Result 型。
 */
template <typename T>
class OrteafResult {
public:
    static OrteafResult success(T value);
    static OrteafResult failure(OrteafErrc errc, std::string message = {});
    static OrteafResult failure(OrteafError error);

    template <typename... Args>
    static OrteafResult failureWith(OrteafErrc errc, Args&&... args);

    bool has_value() const noexcept;
    bool has_error() const noexcept;

    T& value() &;
    const T& value() const&;
    T&& value() &&;

    template <typename U>
    T value_or(U&& default_value) const&;

    OrteafError error() const;

private:
    explicit OrteafResult(detail::OrteafResultImpl<T>&& impl);

    detail::OrteafResultImpl<T> impl_;
};

template <>
class OrteafResult<void> {
public:
    static OrteafResult success();
    static OrteafResult failure(OrteafErrc errc, std::string message = {});
    static OrteafResult failure(OrteafError error);

    bool has_value() const noexcept;
    bool has_error() const noexcept;

    void value() const;
    void value_or() const;

    OrteafError error() const;

private:
    explicit OrteafResult(detail::OrteafResultImpl<void>&& impl);

    detail::OrteafResultImpl<void> impl_;
};

/**
 * @brief 関数を実行し、例外を Result に変換する。
 */
template <typename Fn>
auto captureResult(Fn&& fn) -> OrteafResult<std::invoke_result_t<Fn>>;

/**
 * @brief Result から値を取り出し、失敗している場合は例外を投げる。
 */
template <typename T>
T unwrapOrThrow(OrteafResult<T>&& result);

/// @copydoc unwrapOrThrow(OrteafResult<T>&&)
void unwrapOrThrow(OrteafResult<void>&& result);

/// C 関数ポインタ互換の captureResult。
OrteafResult<void> captureResult(void (*fn)());

}  // namespace orteaf::internal::diagnostics::error

#include "error_impl.h"

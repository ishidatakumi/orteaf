#pragma once

#include <stdexcept>
#include <string>
#include <utility>

#ifdef CUDA_AVAILABLE
  #include <cuda_runtime_api.h>  // 最小限でOK（<cuda_runtime.h>でも可）
  #include <cuda.h>
#endif

namespace orteaf::internal::backend::cuda {

#ifdef CUDA_AVAILABLE

// ランタイムAPI用の例外型
class CudaError : public std::runtime_error {
public:
    explicit CudaError(cudaError_t code,
                       const char* expr,
                       const char* file,
                       int line)
    : std::runtime_error(make_message(code, expr, file, line)),
      code_(code), file_(file), line_(line) {}

    cudaError_t code() const noexcept { return code_; }
    const char* file() const noexcept { return file_; }
    int line() const noexcept { return line_; }

private:
    static std::string make_message(cudaError_t code,
                                    const char* expr,
                                    const char* file,
                                    int line) {
        std::string msg = "CUDA error: ";
        msg += cudaGetErrorString(code);
        msg += " (code ";
        msg += std::to_string(static_cast<int>(code));
        msg += ") while calling ";
        msg += expr;
        msg += " at ";
        msg += file;
        msg += ":";
        msg += std::to_string(line);
        return msg;
    }

    cudaError_t code_;
    const char* file_;
    int line_;
};

// 戻り値チェック（失敗なら例外）
inline void cuda_check(cudaError_t err,
                       const char* expr,
                       const char* file,
                       int line) {
    if (err != cudaSuccess) {
        throw CudaError(err, expr, file, line);
    }
}

// 直近のエラー確認（カーネル起動後などに便利）
inline void cuda_check_last(const char* file, int line) {
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        throw CudaError(err, "cudaGetLastError()", file, line);
    }
}

// デバッグ用：ストリーム同期してエラー顕在化（非同期APIのときに有用）
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

// ドライバAPI用の例外型
class CuDriverError : public std::runtime_error {
public:
    explicit CuDriverError(CUresult code,
                           const char* name,
                           const char* msg,
                           const char* expr,
                           const char* file,
                           int line)
    : std::runtime_error(make_message(code, name, msg, expr, file, line)),
      code_(code), file_(file), line_(line) {}

    CUresult code() const noexcept { return code_; }
    const char* file() const noexcept { return file_; }
    int line() const noexcept { return line_; }

private:
    static std::string make_message(CUresult code,
                                    const char* name,
                                    const char* msg,
                                    const char* expr,
                                    const char* file,
                                    int line) {
        std::string result = "CUDA driver error: ";
        if (name) {
            result += name;
            result += " (";
        } else {
            result += "unknown (";
        }
        result += std::to_string(static_cast<int>(code));
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
        return result;
    }

    CUresult code_;
    const char* file_;
    int line_;
};

inline void cu_driver_check(CUresult err,
                            const char* expr,
                            const char* file,
                            int line) {
    if (err != CUDA_SUCCESS) {
        const char* name = nullptr;
        const char* msg = nullptr;
        cuGetErrorName(err, &name);
        cuGetErrorString(err, &msg);
        throw CuDriverError(err, name, msg, expr, file, line);
    }
}

template <typename Fn>
bool try_driver_call(Fn&& fn) {
    try {
        std::forward<Fn>(fn)();
        return true;
    } catch (const CuDriverError& err) {
        if (err.code() == CUDA_ERROR_DEINITIALIZED) {
            return false;
        }
        throw;
    }
}

#else  // !CUDA_AVAILABLE

// 非 CUDA ビルド：何もしないダミー定義（型にも触れない）
inline void cuda_check(int, const char*, const char*, int) noexcept {}
inline void cuda_check_last(const char*, int) noexcept {}
inline void cuda_check_sync(void*, const char*, int) noexcept {}
inline void cu_driver_check(int, const char*, const char*, int) noexcept {}

template <typename Fn>
bool try_driver_call(Fn&& fn) {
    std::forward<Fn>(fn)();
    return true;
}

#endif // CUDA_AVAILABLE

} // namespace orteaf::internal::backend::cuda

// 使いやすいマクロ
#ifdef CUDA_AVAILABLE
  #define CUDA_CHECK(expr)       ::orteaf::internal::backend::cuda::cuda_check((expr), #expr, __FILE__, __LINE__)
  #define CUDA_CHECK_LAST()      ::orteaf::internal::backend::cuda::cuda_check_last(__FILE__, __LINE__)
  #define CUDA_CHECK_SYNC(s)     ::orteaf::internal::backend::cuda::cuda_check_sync((s), __FILE__, __LINE__)
  #define CU_CHECK(expr)         ::orteaf::internal::backend::cuda::cu_driver_check((expr), #expr, __FILE__, __LINE__)
#else
  #define CUDA_CHECK(expr)       (void)(expr)
  #define CUDA_CHECK_LAST()      ((void)0)
  #define CUDA_CHECK_SYNC(s)     ((void)0)
  #define CU_CHECK(expr)         ((void)0)
#endif

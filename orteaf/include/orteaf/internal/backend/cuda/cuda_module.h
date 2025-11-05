#pragma once

struct CUmodule_st;
using CUmodule_t = CUmodule_st*;

struct CUfunction_st;
using CUfunction_t = CUfunction_st*;

static_assert(sizeof(CUmodule_t) == sizeof(void*), "CUmodule_t must be pointer-sized.");
static_assert(sizeof(CUfunction_t) == sizeof(void*), "CUfunction_t must be pointer-sized.");

namespace orteaf::internal::backend::cuda {

// ファイルパスからモジュールを読み込む（PTX/CUBIN/FATBIN対応）
CUmodule_t load_module_from_file(const char* filepath);

// メモリ上のイメージからモジュールを読み込む（PTX/CUBIN/FATBIN対応）
CUmodule_t load_module_from_image(const void* image);

// モジュールから関数を取得
CUfunction_t get_function(CUmodule_t module, const char* kernel_name);

// モジュールをアンロード
void unload_module(CUmodule_t module);

} // namespace orteaf::internal::backend::cuda

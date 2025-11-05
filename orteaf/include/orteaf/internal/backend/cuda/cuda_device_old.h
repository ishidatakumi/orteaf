#pragma once

//消すから捨てたい後でね

// Handle cases where CUDA_AVAILABLE is defined but set to OFF/0
#if defined(CUDA_AVAILABLE) && !(CUDA_AVAILABLE)
#undef CUDA_AVAILABLE
#endif

#include <iostream>
#include <memory>
#include <vector>
#ifdef CUDA_AVAILABLE
#include <cuda_runtime.h>
#ifdef __CUDACC__
// Launch parameter utilities are implemented inline in this file
#endif
#include <stdexcept>
#include "orteaf/internal/backend/device_backend.h"
#include "cuda_macros.h"
// #include "cuda_buffer_pool.hpp"  // File does not exist
#else
#include <stdexcept>
#include "cuda_macros.h"
#endif

namespace orteaf {
namespace internal {
namespace backend {
namespace cuda {
#ifdef CUDA_AVAILABLE

// ----------------------------------------------------------
// 占有率考慮カーネル設定自動化関数群 (Occupancy-Aware Kernel Launch)
// ----------------------------------------------------------

/**
 * @brief カーネル固有の最適ブロックサイズを取得
 * @tparam KernelFunction カーネル関数型
 * @param kernel カーネル関数ポインタ
 * @param dynamicSMemSize 動的共有メモリサイズ (bytes)
 * @param blockSizeLimit ブロックサイズ上限 (0で制限なし)
 * @return 最適ブロックサイズ
 */
template <typename KernelFunction>
inline int getOptimalBlockSize(KernelFunction kernel, size_t dynamicSMemSize = 0, int blockSizeLimit = 0) {
    int minGridSize, blockSize;
    CUDA_CHECK(cudaOccupancyMaxPotentialBlockSize(&minGridSize, &blockSize, kernel, dynamicSMemSize, blockSizeLimit));
    
    // Ensure block size is within hardware limits (max 1024 threads per block)
    blockSize = std::min(blockSize, 1024);
    
    return blockSize;
}

/**
 * @brief 改良版1Dカーネル用のgridとblockサイズを占有率考慮で自動設定
 * @tparam KernelFunction カーネル関数型
 * @param kernel カーネル関数ポインタ
 * @param totalElements 処理する総要素数
 * @param grid 出力用grid設定
 * @param block 出力用block設定
 * @param dynamicSMemSize 動的共有メモリサイズ (bytes)
 */
template <typename KernelFunction>
inline void autoConfigureKernel(KernelFunction kernel, int total_elements, dim3& grid, dim3& block, size_t dynamicSMemSize = 0) {
    // 占有率を考慮した最適ブロックサイズを取得
    int optimal_block_size = getOptimalBlockSize(kernel, dynamicSMemSize);
    block = dim3(optimal_block_size);

    // 必要なグリッドサイズを計算
    int grid_size = (total_elements + optimal_block_size - 1) / optimal_block_size;
    
    // グリッド次元の自動調整 (1D -> 2D -> 3D)
    if (grid_size <= 65535) {
        // 1D grid
        grid = dim3(grid_size);
    } else if (grid_size <= 65535 * 65535) {
        // 2D grid
        int grid_y = (grid_size + 65535 - 1) / 65535;
        grid = dim3(65535, grid_y);
    } else {
        // 3D grid (極めて大きなテンソル用)
        int grid_y = std::min(65535, (int)sqrt(grid_size / 65535.0) + 1);
        int grid_z = (grid_size + 65535 * grid_y - 1) / (65535 * grid_y);
        grid_z = std::min(grid_z, 65535);
        grid = dim3(65535, grid_y, grid_z);
    }
}

/**
 * @brief 後方互換性のための1Dカーネル設定関数（レガシー）
 * @param total_elements 処理する総要素数
 * @param grid 出力用grid設定
 * @param block 出力用block設定
 */
inline void autoConfigureKernel(int total_elements, dim3& grid, dim3& block) {
    // デフォルトブロックサイズを使用（占有率考慮なし）
    const int default_block_size = 256;
    block = dim3(default_block_size);

    // 必要なグリッドサイズを計算
    int grid_size = (total_elements + default_block_size - 1) / default_block_size;
    
    // グリッド次元の自動調整
    if (grid_size <= 65535) {
        grid = dim3(grid_size);
    } else if (grid_size <= 65535 * 65535) {
        int grid_y = (grid_size + 65535 - 1) / 65535;
        grid = dim3(65535, grid_y);
    } else {
        int grid_y = std::min(65535, (int)sqrt(grid_size / 65535.0) + 1);
        int grid_z = (grid_size + 65535 * grid_y - 1) / (65535 * grid_y);
        grid_z = std::min(grid_z, 65535);
        grid = dim3(65535, grid_y, grid_z);
    }
}

/**
 * @brief 占有率考慮2Dカーネル用のgridとblockサイズを自動設定
 * @tparam KernelFunction カーネル関数型
 * @param kernel カーネル関数ポインタ
 * @param M 行数
 * @param N 列数
 * @param batch バッチサイズ
 * @param grid 出力用grid設定
 * @param block 出力用block設定
 * @param dynamicSMemSize 動的共有メモリサイズ (bytes)
 */
template <typename KernelFunction>
inline void autoConfigureKernel2D(KernelFunction kernel, int M, int N, int batch, dim3& grid, dim3& block, size_t dynamicSMemSize = 0) {
    // 占有率を考慮した最適ブロックサイズを取得
    int optimal_block_size = getOptimalBlockSize(kernel, dynamicSMemSize);
    
    // 2D問題に適した2D block配置を決定
    int blockX, blockY;
    if (optimal_block_size >= 1024) {
        blockX = blockY = 32;  // 32x32 = 1024
    } else if (optimal_block_size >= 512) {
        blockX = blockY = std::min(N, std::min(M, 22));  // ~22x22 = 484
        if (blockX * blockY > optimal_block_size) {
            blockX = blockY = 16;  // 16x16 = 256
        }
    } else if (optimal_block_size >= 256) {
        blockX = blockY = 16;  // 16x16 = 256
    } else {
        blockX = blockY = 8;   // 8x8 = 64 (minimum practical 2D block)
    }
    
    // Adjust for actual problem dimensions
    blockX = std::min(blockX, N);
    blockY = std::min(blockY, M);
    
    block = dim3(blockX, blockY, 1);
    grid = dim3((N + blockX - 1) / blockX, (M + blockY - 1) / blockY, batch);
}

/**
 * @brief 後方互換性のための2Dカーネル設定関数（レガシー）
 * @param M 行数
 * @param N 列数
 * @param batch バッチサイズ
 * @param grid 出力用grid設定
 * @param block 出力用block設定
 */
inline void autoConfigureKernel2D(int M, int N, int batch, dim3& grid, dim3& block) {
    // デフォルトで32×32のブロックサイズを使用（占有率考慮なし）
    int blockX = std::min(32, N);
    int blockY = std::min(32, M);
    block = dim3(blockX, blockY, 1);
    grid = dim3((N + blockX - 1) / blockX, (M + blockY - 1) / blockY, batch);
}

/**
 * @brief 占有率考慮3Dカーネル用のgridとblockサイズを自動設定
 * @tparam KernelFunction カーネル関数型
 * @param kernel カーネル関数ポインタ
 * @param M ミニバッチサイズ
 * @param N 出力特徴数
 * @param Z JBB サイズまたは深度次元
 * @param grid 出力用grid設定
 * @param block 出力用block設定
 * @param dynamicSMemSize 動的共有メモリサイズ (bytes)
 */
template <typename KernelFunction>
inline void autoConfigureKernel3D(KernelFunction kernel, int M, int N, int Z, dim3& grid, dim3& block, size_t dynamicSMemSize = 0) {
    // 占有率を考慮した最適ブロックサイズを取得
    int optimal_block_size = getOptimalBlockSize(kernel, dynamicSMemSize);
    
    // 3D問題に適したblock配置を決定
    int blockX, blockY, blockZ;
    if (optimal_block_size >= 1024) {
        // 大きなブロック: 1D配置（最も効率的）
        blockX = std::min(N, optimal_block_size);
        blockY = blockZ = 1;
    } else if (optimal_block_size >= 256) {
        // 中程度: 2D配置
        blockX = std::min(N, 16);
        blockY = std::min(M, optimal_block_size / blockX);
        blockZ = 1;
    } else {
        // 小さなブロック: 3D配置
        blockX = std::min(N, 8);
        blockY = std::min(M, 8);
        blockZ = std::min(Z, optimal_block_size / (blockX * blockY));
        if (blockZ < 1) blockZ = 1;
    }
    
    block = dim3(blockX, blockY, blockZ);
    grid = dim3((N + blockX - 1) / blockX, (M + blockY - 1) / blockY, (Z + blockZ - 1) / blockZ);
}

/**
 * @brief 後方互換性のための3Dカーネル設定関数（レガシー、JBB専用）
 * @param M ミニバッチサイズ
 * @param N 出力特徴数
 * @param Z JBB サイズ
 * @param grid 出力用grid設定
 * @param block 出力用block設定
 */
inline void autoConfigureKernel3D(int M, int N, int Z, dim3& grid, dim3& block) {
    // X 方向は最大 256 スレッド（占有率考慮なし）
    int blockX = std::min(N, 256);
    block = dim3(blockX, 1, 1);
    grid = dim3((N + blockX - 1) / blockX, M, Z);
}

/**
 * @brief JBB専用カーネル設定 (拡張版)
 * @param jbb_shape JBB形状
 * @param tensor_shape テンソル形状
 * @param grid 出力用grid設定
 * @param block 出力用block設定
 */
inline void autoConfigureJBBKernel(const std::vector<int>& jbb_shape,
                                        const std::vector<int>& tensor_shape,
                                        dim3& grid,
                                        dim3& block) {
    if (jbb_shape.size() == 1 && tensor_shape.size() >= 2) {
        // JBB 1D + Tensor 2D+の場合
        autoConfigureKernel3D(tensor_shape[0], tensor_shape[1], jbb_shape[0], grid, block);
    } else if (jbb_shape.size() == 2 && tensor_shape.size() >= 1) {
        // JBB 2D + Tensor 1D+の場合
        int total_jbb = jbb_shape[0] * jbb_shape[1];
        int total_tensor = 1;
        for (int dim : tensor_shape)
            total_tensor *= dim;

        block = dim3(256, 1, 1);
        grid = dim3((total_tensor + 255) / 256, total_jbb, 1);
    } else {
        // デフォルト: 総要素数ベース
        int total_elements = 1;
        for (int dim : jbb_shape)
            total_elements *= dim;
        for (int dim : tensor_shape)
            total_elements *= dim;

        autoConfigureKernel(total_elements, grid, block);
    }
}

/**
 * @brief CUDA memory management utilities
 */
template <typename T>
class CudaMemory {
  public:
    /**
     * @brief デバイスメモリ確保
     */
    static T* allocate(size_t count) {
        T* ptr = nullptr;
        CUDA_CHECK(cudaMalloc(&ptr, count * sizeof(T)));
        return ptr;
    }

    /**
     * @brief デバイスメモリ解放
     */
    static void deallocate(T* ptr) {
        if (ptr) {
            CUDA_CHECK(cudaFree(ptr));
        }
    }

    /**
     * @brief ホスト→デバイス転送
     */
    static void copy_to_device(T* d_ptr, const T* h_ptr, size_t count) {
        CUDA_CHECK(cudaMemcpy(d_ptr, h_ptr, count * sizeof(T), cudaMemcpyHostToDevice));
    }

    /**
     * @brief デバイス→ホスト転送
     */
    static void copy_to_host(T* h_ptr, const T* d_ptr, size_t count) {
        CUDA_CHECK(cudaMemcpy(h_ptr, d_ptr, count * sizeof(T), cudaMemcpyDeviceToHost));
    }

    /**
     * @brief デバイスメモリゼロ初期化
     */
    static void zero(T* d_ptr, size_t count) {
        CUDA_CHECK(cudaMemset(d_ptr, 0, count * sizeof(T)));
    }
};

/**
 * @brief CUDA device information
 */
class CudaInfo {
  public:
    /**
     * @brief 利用可能なデバイス数を取得
     */
    static int getDeviceCount() {
        int count = 0;
        CUDA_CHECK(cudaGetDeviceCount(&count));
        return count;
    }

    /**
     * @brief 現在のデバイスIDを取得
     */
    static int getCurrentDevice() {
        int device = 0;
        CUDA_CHECK(cudaGetDevice(&device));
        return device;
    }

    /**
     * @brief デバイスを設定
     */
    static void setDevice(int device_id) {
        CUDA_CHECK(cudaSetDevice(device_id));
    }

    /**
     * @brief デバイス同期
     */
    static void synchronize() {
        CUDA_CHECK(cudaDeviceSynchronize());
    }

    /**
     * @brief デバイス名を取得
     */
    static std::string getDeviceName(int device_id) {
        cudaDeviceProp prop;
        CUDA_CHECK(cudaGetDeviceProperties(&prop, device_id));
        return std::string(prop.name);
    }

    /**
     * @brief デバイスの総メモリ量を取得
     */
    static size_t getTotalMemory(int device_id) {
        cudaDeviceProp prop;
        CUDA_CHECK(cudaGetDeviceProperties(&prop, device_id));
        return prop.totalGlobalMem;
    }

    /**
     * @brief デバイスの空きメモリ量を取得
     */
    static size_t getFreeMemory(int device_id) {
        int current_device = getCurrentDevice();
        setDevice(device_id);

        size_t free_mem, total_mem;
        CUDA_CHECK(cudaMemGetInfo(&free_mem, &total_mem));

        setDevice(current_device);
        return free_mem;
    }

    /**
     * @brief デバイスの使用メモリ量を取得
     */
    static size_t getUsedMemory(int device_id) {
        size_t total = getTotalMemory(device_id);
        size_t free = getFreeMemory(device_id);
        return total - free;
    }

    /**
     * @brief デバイス使用率を取得（簡易版）
     */
    static float getUtilization(int device_id) {
        size_t total = getTotalMemory(device_id);
        size_t used = getUsedMemory(device_id);
        return (float)used / (float)total * 100.0f;
    }

    /**
     * @brief デバイスプロパティを取得
     */
    static cudaDeviceProp getDeviceProperties(int device_id) {
        cudaDeviceProp prop;
        CUDA_CHECK(cudaGetDeviceProperties(&prop, device_id));
        return prop;
    }
};

#ifdef __CUDACC__
// ----------------------------------------------------------
// atomicAdd for signed 64bit (CUDA は unsigned long long のみ提供なのでラップ)
// ----------------------------------------------------------
#ifndef __CUDA_ARCH__  // will still be seen by host compiler through nvcc; guard redundant.
#endif
#if defined(__CUDA_ARCH__)
// (removed duplicate atomicAdd signed 64bit overload, see global version)
#endif

template <typename T>
__device__ inline void CudaAdd(T* ptr, T value) {
    if constexpr (std::is_same_v<T, long long>) {
        // CUDA は signed 64bit を直接サポートしないため unsigned へ変換
        ::atomicAdd(reinterpret_cast<unsigned long long*>(ptr), static_cast<unsigned long long>(value));
    } else if constexpr (std::is_same_v<T, int> || std::is_same_v<T, unsigned int> ||
                         std::is_same_v<T, unsigned long long> || std::is_same_v<T, float> ||
                         std::is_same_v<T, double>) {
        ::atomicAdd(ptr, value);
    } else if constexpr (std::is_same_v<T, long>) {
#if __SIZEOF_LONG__ == 8
        ::atomicAdd(reinterpret_cast<unsigned long long*>(ptr), static_cast<unsigned long long>(value));
#else  // 32-bit long (Windows 64bit/LLP64 etc.)
        ::atomicAdd(reinterpret_cast<unsigned int*>(ptr), static_cast<unsigned int>(value));
#endif
    } else {
        // atomicAdd が未対応の型（例えば half など）はフォールバック
        *ptr += value;
    }
}

template <typename T, typename S>
__device__ inline void CudaAdd(T* ptr, S value, typename std::enable_if_t<!std::is_same_v<T, S>, int> = 0) {
    CudaAdd(ptr, static_cast<T>(value));
}

// note: 64bit 整数は汎用版 (long long) ブランチで処理
#else  // __CUDACC__ が未定義 = ホストビルド

template <typename T>
inline void CudaAdd(T* ptr, T value) {
    *ptr += value;
}

template <typename T, typename S>
inline void CudaAdd(T* ptr, S value, typename std::enable_if_t<!std::is_same_v<T, S>, int> = 0) {
    *ptr += static_cast<T>(value);
}

#endif  // __CUDACC__

// -------------- signed 64bit atomicAdd (global) -----------------
#if defined(__CUDA_ARCH__)
// duplicate definition removed; global overload is provided above.
#endif

#endif  // CUDA_AVAILABLE

// デバイス管理用の便利関数
inline void setDevice(int device_id) {
#ifdef CUDA_AVAILABLE
    CudaInfo::setDevice(device_id);
#endif
}

}  // namespace cuda
}  // namespace device
}  // namespace core
}  // namespace orteaf

// ----------------------------------------------------------
// CUDA が無効の場合のスタブ定義
// ----------------------------------------------------------
#ifndef CUDA_AVAILABLE
#include <cstdlib>  // std::malloc / std::free 用
#include <cstring>  // std::memcpy 用

// CUDA エラー型と戻り値
using cudaError_t = int;
constexpr cudaError_t cudaSuccess = 0;

// dim3 構造体（CUDA 依存コードで使用）
struct dim3 {
    unsigned int x{1}, y{1}, z{1};
    constexpr dim3(unsigned int vx = 1, unsigned int vy = 1, unsigned int vz = 1) : x(vx), y(vy), z(vz) {}
};

// cudaMemcpyKind 列挙体
enum cudaMemcpyKind {
    cudaMemcpyHostToHost = 0,
    cudaMemcpyHostToDevice = 1,
    cudaMemcpyDeviceToHost = 2,
    cudaMemcpyDeviceToDevice = 3,
    cudaMemcpyDefault = 4
};

// スタブ API
inline const char* cudaGetErrorString(cudaError_t) {
    return "CUDA stub";
}

inline cudaError_t cudaMalloc(void** devPtr, size_t size) {
    // ホストメモリ確保で代替
    *devPtr = (size > 0) ? std::malloc(size) : nullptr;
    return cudaSuccess;
}

inline cudaError_t cudaFree(void* devPtr) {
    std::free(devPtr);
    return cudaSuccess;
}

inline cudaError_t cudaMemcpy(void* dst, const void* src, size_t count, cudaMemcpyKind) {
    std::memcpy(dst, src, count);
    return cudaSuccess;
}

inline cudaError_t cudaMemset(void* devPtr, int value, size_t count) {
    std::memset(devPtr, value, count);
    return cudaSuccess;
}

inline cudaError_t cudaGetDeviceCount(int* count) {
    if (count)
        *count = 0;
    return cudaSuccess;
}

inline cudaError_t cudaGetDevice(int* device) {
    if (device) 
        *device = 0;
    return cudaSuccess;
}

inline cudaError_t cudaSetDevice(int) {
    return cudaSuccess;
}

inline cudaError_t cudaDeviceSynchronize() {
    return cudaSuccess;
}

// CUDA 関数修飾子を空定義
#ifndef __host__
#define __host__
#endif
#ifndef __device__
#define __device__
#endif
#ifndef __global__
#define __global__
#endif
#ifndef __shared__
#define __shared__
#endif

#endif  // CUDA_AVAILABLE スタブ終端

#ifdef __CUDACC__
#if defined(__CUDA_ARCH__)
// -----------------------------------------------------------------
// グローバル作用域の signed 64bit atomicAdd (CUDA 公式には unsigned 版のみ)
// どの名前空間からでも見えるように早期に定義しておく。
// -----------------------------------------------------------------
// (removed duplicate atomicAdd signed 64bit overload, see global version)
#endif
#endif

#if defined(__CUDA_ARCH__)
// -----------------------------------------------------------------
// グローバル作用域 signed 64bit atomicAdd 定義
// CUDA 標準では unsigned long long* 版のみのためラップを提供
// -----------------------------------------------------------------
__device__ inline long long atomicAdd(long long* address, long long val) {
    return static_cast<long long>(
        ::atomicAdd(reinterpret_cast<unsigned long long*>(address), static_cast<unsigned long long>(val)));
}

__device__ inline long atomicAdd(long* address, long val) {
    return static_cast<long>(
        ::atomicAdd(reinterpret_cast<unsigned long long*>(address), static_cast<unsigned long long>(val)));
}
#endif

// --------------------------------------------------------------------
// Forward declarations for signed 64-bit and LP64 signed long atomicAdd.
// CUDA は unsigned long long* 版のみ公式に提供しているため、
// ここで signed 版を宣言しておき後段でインライン実装を定義する。
// --------------------------------------------------------------------
__device__ long atomicAdd(long* address, long val);
__device__ long long atomicAdd(long long* address, long long val);

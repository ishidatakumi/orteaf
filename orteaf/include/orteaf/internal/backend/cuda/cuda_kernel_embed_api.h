#pragma once

#include <cstddef>
#include <string_view>

#ifndef ORTEAF_EMBED_HAS_FATBIN
#define ORTEAF_EMBED_HAS_FATBIN 0
#endif
#ifndef ORTEAF_EMBED_HAS_CUBIN
#define ORTEAF_EMBED_HAS_CUBIN 0
#endif
#ifndef ORTEAF_EMBED_HAS_PTX
#define ORTEAF_EMBED_HAS_PTX 0
#endif

namespace orteaf::internal::backend::cuda::kernel_embed {

enum class KernelFmt { Fatbin, Cubin, Ptx };

constexpr const char* to_string(KernelFmt fmt) noexcept {
    switch (fmt) {
    case KernelFmt::Fatbin: return "fatbin";
    case KernelFmt::Cubin: return "cubin";
    case KernelFmt::Ptx: return "ptx";
    }
    return "unknown";
}

struct Blob {
    const void* data;
    std::size_t size;
};

Blob find_kernel_data(std::string_view name,
                      KernelFmt prefer = KernelFmt::Fatbin,
                      Blob fallback = {nullptr, 0});

bool available(std::string_view name, KernelFmt fmt);

} // namespace orteaf::internal::backend::cuda::kernel_embed

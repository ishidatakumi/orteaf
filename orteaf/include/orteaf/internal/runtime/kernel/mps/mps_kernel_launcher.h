#pragma once

#if ORTEAF_ENABLE_MPS

#include <initializer_list>
#include <memory>
#include <utility>

#include "orteaf/internal/runtime/kernel/mps/mps_kernel_launcher_impl.h"

namespace orteaf::internal::runtime::mps {

/**
 * Lightweight wrapper that owns MpsKernelLauncherImpl<N> via pimpl.
 * Users pass library/function pairs as string literals and call launch(index, encoder).
 */
template <std::size_t N>
class MpsKernelLauncher {
public:
    using Impl = MpsKernelLauncherImpl<N>;
    using PipelineLease = typename Impl::PipelineLease;
    using KeyLiteral = typename Impl::KeyLiteral;

    explicit MpsKernelLauncher(std::initializer_list<KeyLiteral> keys)
        : impl_(std::make_unique<Impl>(keys)) {}

    MpsKernelLauncher(const MpsKernelLauncher&) = delete;
    MpsKernelLauncher& operator=(const MpsKernelLauncher&) = delete;
    MpsKernelLauncher(MpsKernelLauncher&&) = default;
    MpsKernelLauncher& operator=(MpsKernelLauncher&&) = default;
    virtual ~MpsKernelLauncher() = default;

#if ORTEAF_ENABLE_TEST
    const Impl& implForTest() const noexcept { return *impl_; }
    Impl& implForTest() noexcept { return *impl_; }
#endif

protected:
    std::unique_ptr<Impl> impl_{};
};

}  // namespace orteaf::internal::runtime::mps

#endif  // ORTEAF_ENABLE_MPS

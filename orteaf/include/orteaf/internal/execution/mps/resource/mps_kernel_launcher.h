#pragma once

#if ORTEAF_ENABLE_MPS

#include <initializer_list>
#include <memory>
#include <utility>

#include "orteaf/internal/execution/mps/resource/mps_kernel_launcher_impl.h"

namespace orteaf::internal::execution::mps::resource {

/**
 * Lightweight wrapper that owns MpsKernelLauncherImpl<N> via pimpl.
 * Users pass library/function pairs as string literals and call launch(index,
 * encoder).
 */
template <std::size_t N> class MpsKernelLauncher {
public:
  using Impl = MpsKernelLauncherImpl<N>;
  using PipelineLease = typename Impl::PipelineLease;
  using KeyLiteral = typename Impl::KeyLiteral;

  MpsKernelLauncher() = default;

  explicit MpsKernelLauncher(std::initializer_list<KeyLiteral> keys)
      : impl_(std::make_unique<Impl>(keys)) {}

  MpsKernelLauncher(const MpsKernelLauncher &) = delete;
  MpsKernelLauncher &operator=(const MpsKernelLauncher &) = delete;
  MpsKernelLauncher(MpsKernelLauncher &&) = default;
  MpsKernelLauncher &operator=(MpsKernelLauncher &&) = default;
  virtual ~MpsKernelLauncher() = default;

private:
  std::unique_ptr<Impl> impl_{};

public:
  // Forward commonly-used methods to impl_
  template <typename... Args>
  auto initialized(Args &&...args) const
      -> decltype(impl_->initialized(std::forward<Args>(args)...)) {
    return impl_->initialized(std::forward<Args>(args)...);
  }

  template <typename PrivateOps, typename... Args>
  auto initialize(Args &&...args)
      -> decltype(impl_->template initialize<PrivateOps>(
          std::forward<Args>(args)...)) {
    return impl_->template initialize<PrivateOps>(std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto createCommandBuffer(Args &&...args) const
      -> decltype(impl_->createCommandBuffer(std::forward<Args>(args)...)) {
    return impl_->createCommandBuffer(std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto createComputeEncoder(Args &&...args) const
      -> decltype(impl_->createComputeEncoder(std::forward<Args>(args)...)) {
    return impl_->createComputeEncoder(std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto setBuffer(Args &&...args) const
      -> decltype(impl_->setBuffer(std::forward<Args>(args)...)) {
    return impl_->setBuffer(std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto setBytes(Args &&...args) const
      -> decltype(impl_->setBytes(std::forward<Args>(args)...)) {
    return impl_->setBytes(std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto dispatchThreadgroups(Args &&...args) const
      -> decltype(impl_->dispatchThreadgroups(std::forward<Args>(args)...)) {
    return impl_->dispatchThreadgroups(std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto endEncoding(Args &&...args) const
      -> decltype(impl_->endEncoding(std::forward<Args>(args)...)) {
    return impl_->endEncoding(std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto commit(Args &&...args) const
      -> decltype(impl_->commit(std::forward<Args>(args)...)) {
    return impl_->commit(std::forward<Args>(args)...);
  }

#if ORTEAF_ENABLE_TEST
  const Impl &implForTest() const noexcept { return *impl_; }
  Impl &implForTest() noexcept { return *impl_; }
#endif
};

} // namespace orteaf::internal::execution::mps::resource

#endif // ORTEAF_ENABLE_MPS

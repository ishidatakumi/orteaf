#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <gmock/gmock.h>

#include <orteaf/internal/backend/backend.h>
#include <orteaf/internal/base/handle.h>
#include <orteaf/internal/base/heap_vector.h>
#include <orteaf/internal/execution/allocator/buffer_resource.h>
#include <orteaf/internal/execution/base/backend_traits.h>
#include <orteaf/internal/execution/mps/platform/wrapper/mps_buffer.h>
#include <orteaf/internal/execution/mps/resource/mps_buffer_view.h>
#include <orteaf/internal/execution/mps/resource/mps_reuse_token.h>

namespace orteaf::tests::execution::mps {

/**
 * @brief テスト用のシンプルな MpsResource スタブ実装
 *
 * GMock を使わずに MpsResource のインターフェースを満たすシンプルな実装。
 * SegregatePool やその他のポリシーで使用できる。
 */
class StubMpsResource {
public:
  using BufferView = ::orteaf::internal::execution::mps::resource::MpsBufferView;
  using BufferResource = ::orteaf::internal::execution::allocator::BufferResource<
      ::orteaf::internal::backend::Backend::Mps>;
  using BufferBlock = ::orteaf::internal::execution::allocator::BufferBlock<
      ::orteaf::internal::backend::Backend::Mps>;
  using FenceToken = ::orteaf::internal::execution::mps::resource::MpsFenceToken;
  using ReuseToken = ::orteaf::internal::execution::mps::resource::MpsReuseToken;
  using LaunchParams = ::orteaf::internal::execution::base::BackendTraits<
      ::orteaf::internal::backend::Backend::Mps>::KernelLaunchParams;
  using MpsBuffer_t =
      ::orteaf::internal::execution::mps::platform::wrapper::MpsBuffer_t;

  struct Config {
    ::orteaf::internal::base::DeviceHandle device_handle{};
    void *device{nullptr};
    void *heap{nullptr};
    std::size_t chunk_table_capacity{16};
    // For compatibility with MpsBufferManager
    int usage{0};                   // Stub - MpsResource has MpsBufferUsage_t
    void *library_manager{nullptr}; // Stub - MpsResource has MpsLibraryManager*
  };

  static constexpr ::orteaf::internal::backend::Backend
  backend_type_static() noexcept {
    return ::orteaf::internal::backend::Backend::Mps;
  }

  StubMpsResource() = default;
  ~StubMpsResource() = default;

  // Non-copyable
  StubMpsResource(const StubMpsResource &) = delete;
  StubMpsResource &operator=(const StubMpsResource &) = delete;

  // Movable (required by SegregatePool constructor)
  StubMpsResource(StubMpsResource &&other) noexcept
      : initialized_(other.initialized_),
        allocate_count_(other.allocate_count_),
        deallocate_count_(other.deallocate_count_) {
    other.initialized_ = false;
    other.allocate_count_ = 0;
    other.deallocate_count_ = 0;
  }

  StubMpsResource &operator=(StubMpsResource &&other) noexcept {
    if (this != &other) {
      initialized_ = other.initialized_;
      allocate_count_ = other.allocate_count_;
      deallocate_count_ = other.deallocate_count_;
      other.initialized_ = false;
      other.allocate_count_ = 0;
      other.deallocate_count_ = 0;
    }
    return *this;
  }

  void initialize(const Config &config) {
    initialized_ = true;
    allocate_count_ = 0;
    deallocate_count_ = 0;
  }

  BufferView allocate(std::size_t size, std::size_t alignment) {
    if (!initialized_ || size == 0) {
      return {};
    }
    ++allocate_count_;
    // Return a fake buffer view with a unique "handle" based on allocation
    // count
    MpsBuffer_t fake_buffer =
        reinterpret_cast<MpsBuffer_t>(0x1000 + allocate_count_ * 0x1000);
    return BufferView{fake_buffer, 0, size};
  }

  void deallocate(BufferView view, std::size_t size,
                  std::size_t alignment) noexcept {
    if (view) {
      ++deallocate_count_;
    }
  }

  bool isCompleted(FenceToken &token) { return true; }
  bool isCompleted(ReuseToken &token) { return true; }

  static BufferView makeView(BufferView base, std::size_t offset,
                             std::size_t size) {
    return BufferView{base.raw(), offset, size};
  }

  void initializeChunkAsFreelist(std::size_t, BufferView, std::size_t,
                                 std::size_t, const LaunchParams & = {}) {}
  BufferView popFreelistNode(std::size_t, const LaunchParams & = {}) {
    return {};
  }
  void pushFreelistNode(std::size_t, BufferView, const LaunchParams & = {}) {}

  // Test accessors
  std::size_t allocateCount() const { return allocate_count_; }
  std::size_t deallocateCount() const { return deallocate_count_; }
  bool isInitialized() const { return initialized_; }

private:
  bool initialized_{false};
  std::size_t allocate_count_{0};
  std::size_t deallocate_count_{0};
};

} // namespace orteaf::tests::execution::mps

#endif // ORTEAF_ENABLE_MPS

#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>

#include <orteaf/internal/base/handle.h>
#include <orteaf/internal/base/heap_vector.h>
#include <orteaf/internal/runtime/allocator/buffer_resource.h>
#include <orteaf/internal/runtime/mps/manager/mps_command_queue_manager.h>
#include <orteaf/internal/runtime/mps/manager/mps_library_manager.h>
#include <orteaf/internal/runtime/mps/platform/wrapper/mps_buffer.h>
#include <orteaf/internal/runtime/mps/platform/wrapper/mps_command_queue.h>
#include <orteaf/internal/runtime/mps/platform/wrapper/mps_device.h>
#include <orteaf/internal/runtime/mps/platform/wrapper/mps_heap.h>
#include <orteaf/internal/runtime/mps/resource/mps_buffer_view.h>
#include <orteaf/internal/runtime/mps/resource/mps_fence_token.h>
#include <orteaf/internal/runtime/mps/resource/mps_reuse_token.h>
#include <unordered_map>

namespace orteaf::internal::runtime::allocator::resource::mps {

// Simple MPS resource that keeps device/heap handles per instance and creates
// buffers at offset 0.
class MpsResource {
public:
  using BufferView = ::orteaf::internal::runtime::mps::resource::MpsBufferView;
  using BufferResource = ::orteaf::internal::runtime::allocator::BufferResource<
      ::orteaf::internal::backend::Backend::Mps>;
  using FenceToken = ::orteaf::internal::runtime::mps::resource::MpsFenceToken;
  using ReuseToken = ::orteaf::internal::runtime::mps::resource::MpsReuseToken;
  using MPSBuffer_t =
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSBuffer_t;
  struct LaunchParams {
    ::orteaf::internal::base::DeviceHandle device_handle{};
    ::orteaf::internal::runtime::mps::manager::MpsCommandQueueManager::
        CommandQueueLease command_queue{};
  };

  struct Config {
    ::orteaf::internal::base::DeviceHandle device_handle{};
    ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t device{
        nullptr};
    ::orteaf::internal::runtime::mps::platform::wrapper::MPSHeap_t heap{
        nullptr};
    ::orteaf::internal::runtime::mps::platform::wrapper::MPSHeap_t staging_heap{
        nullptr}; // optional host-visible heap for readback
    ::orteaf::internal::runtime::mps::platform::wrapper::MPSBufferUsage_t usage{
        ::orteaf::internal::runtime::mps::platform::wrapper::
            kMPSDefaultBufferUsage};
    ::orteaf::internal::runtime::mps::platform::wrapper::MPSBufferUsage_t
        staging_usage{::orteaf::internal::runtime::mps::platform::wrapper::
                          kMPSDefaultBufferUsage};
    ::orteaf::internal::runtime::mps::manager::MpsLibraryManager
        *library_manager{nullptr};
    std::size_t chunk_table_capacity{16};
  };

  MpsResource() = default;

  explicit MpsResource(const Config &config) { initialize(config); }

  // Copy is deleted
  MpsResource(const MpsResource &) = delete;
  MpsResource &operator=(const MpsResource &) = delete;

  // Move is allowed
  MpsResource(MpsResource &&) = default;
  MpsResource &operator=(MpsResource &&) = default;

  ~MpsResource() = default;

  void initialize(const Config &config);

  BufferView allocate(std::size_t size, std::size_t alignment);

  void deallocate(BufferView view, std::size_t size,
                  std::size_t alignment) noexcept;

  ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t
  device() const noexcept {
    return device_;
  }
  ::orteaf::internal::runtime::mps::platform::wrapper::MPSHeap_t
  heap() const noexcept {
    return heap_;
  }

  bool isCompleted(FenceToken &token);
  bool isCompleted(ReuseToken &token);

  static BufferView makeView(BufferView base, std::size_t offset,
                             std::size_t size);

  void initializeChunkAsFreelist(std::size_t list_index, BufferView chunk,
                                 std::size_t chunk_size, std::size_t block_size,
                                 const LaunchParams &launch_params = {});
  BufferView popFreelistNode(std::size_t list_index,
                             const LaunchParams &launch_params = {});
  void pushFreelistNode(std::size_t list_index, BufferView view,
                        const LaunchParams &launch_params = {});

private:
  void destroyFreelist();
  void ensureList(std::size_t list_index);

  ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t device_{
      nullptr};
  ::orteaf::internal::base::DeviceHandle device_handle_{};
  ::orteaf::internal::runtime::mps::platform::wrapper::MPSHeap_t heap_{nullptr};
  ::orteaf::internal::runtime::mps::platform::wrapper::MPSBufferUsage_t usage_{
      ::orteaf::internal::runtime::mps::platform::wrapper::
          kMPSDefaultBufferUsage};
  bool initialized_{false};
};

} // namespace orteaf::internal::runtime::allocator::resource::mps

#endif // ORTEAF_ENABLE_MPS

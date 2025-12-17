#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/runtime/mps/platform/wrapper/metal_kernel_embed_api.h"

#include "orteaf/backend_embed/mps/metal_kernel_registry_entries.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_function.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_library.h"

namespace orteaf::internal::runtime::mps::platform::metal_kernel_embed {

std::span<const MetallibEntry> libraries() { return kMetalKernelEntries; }

MetallibBlob findLibraryData(std::string_view library_name) {
  for (const MetallibEntry &entry : kMetalKernelEntries) {
    if (std::string_view(entry.name) == library_name) {
      return {entry.begin, static_cast<std::size_t>(entry.end - entry.begin)};
    }
  }
  return {nullptr, 0};
}

bool available(std::string_view library_name) {
  MetallibBlob blob = findLibraryData(library_name);
  return blob.data != nullptr && blob.size > 0;
}

::orteaf::internal::runtime::mps::platform::wrapper::MpsLibrary_t
createEmbeddedLibrary(
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t device,
    std::string_view library_name,
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsError_t *error) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
  if (!device) {
    if (error) {
      *error = ::orteaf::internal::runtime::mps::platform::wrapper::createError(
          "orteaf.mps.kernel", "Invalid MPS device");
    }
    return nil;
  }
  MetallibBlob blob = findLibraryData(library_name);
  if (blob.data == nullptr || blob.size == 0) {
    if (error) {
      *error = ::orteaf::internal::runtime::mps::platform::wrapper::createError(
          "orteaf.mps.kernel", "Embedded metallib not found");
    }
    return nil;
  }
  ::orteaf::internal::runtime::mps::platform::wrapper::MpsError_t local_error =
      nullptr;
  ::orteaf::internal::runtime::mps::platform::wrapper::MpsLibrary_t library =
      ::orteaf::internal::runtime::mps::platform::wrapper::
          createLibraryWithData(device, blob.data, blob.size, &local_error);
  if (library == nil) {
    if (error) {
      *error = local_error;
    } else if (local_error) {
      ::orteaf::internal::runtime::mps::platform::wrapper::destroyError(
          local_error);
    }
    return nil;
  }
  if (local_error) {
    ::orteaf::internal::runtime::mps::platform::wrapper::destroyError(
        local_error);
  }
  if (error) {
    *error = nullptr;
  }
  return library;
#else
  (void)device;
  (void)library_name;
  (void)error;
  return nullptr;
#endif
}

} // namespace orteaf::internal::runtime::mps::platform::metal_kernel_embed

#endif // ORTEAF_ENABLE_MPS

#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>

#include "orteaf/internal/execution/mps/platform/wrapper/mps_types.h"

namespace orteaf::internal::execution::mps::platform::wrapper {

[[nodiscard]] MpsLibrary_t createLibrary(MpsDevice_t device, MpsString_t name,
                                        MpsError_t *error = nullptr);

[[nodiscard]] MpsLibrary_t createLibraryWithSource(
    MpsDevice_t device, MpsString_t source, MpsCompileOptions_t compile_options,
    MpsError_t *error = nullptr);

[[nodiscard]] MpsLibrary_t createLibraryWithData(MpsDevice_t device,
                                                 const void *data,
                                                 std::size_t size,
                                                 MpsError_t *error = nullptr);

void destroyLibrary(MpsLibrary_t library);

} // namespace orteaf::internal::execution::mps::platform::wrapper

#endif  // ORTEAF_ENABLE_MPS

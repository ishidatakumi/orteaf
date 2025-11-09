#pragma once

#include "orteaf/internal/backend/mps/mps_device.h"
#include "orteaf/internal/backend/mps/mps_function.h"
#include "orteaf/internal/backend/mps/mps_error.h"
#include "orteaf/internal/backend/mps/mps_library.h"

#include <cstddef>
#include <string_view>

namespace orteaf::internal::backend::mps::metal_kernel_embed {

struct MetallibBlob {
    const void* data;
    std::size_t size;
};

MetallibBlob find_library_data(std::string_view library_name);

bool available(std::string_view library_name);

MPSFunction_t load_embedded_function(MPSDevice_t device,
                                     std::string_view library_name,
                                     std::string_view function_name,
                                     MPSError_t* error = nullptr);

} // namespace orteaf::internal::backend::mps::metal_kernel_embed

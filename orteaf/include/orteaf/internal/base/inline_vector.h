#pragma once

#include <cstdint>

namespace orteaf::internal::base {

template <typename T, std::uint8_t N>
struct InlineVector {
    T data[N]{};
    std::uint8_t size = 0;
};

}  // namespace orteaf::internal::base


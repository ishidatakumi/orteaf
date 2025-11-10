#pragma once

#include <bit>
#include <type_traits>

namespace orteaf::internal::detail {

template <class To, class From>
constexpr To bitCast(From value) {
#if defined(__cpp_lib_bit_cast)
    return std::bit_cast<To>(value);
#else
    static_assert(sizeof(To) == sizeof(From), "BitCast requires identical sizes");
    static_assert(std::is_trivially_copyable_v<To>, "BitCast requires trivially copyable types");
    static_assert(std::is_trivially_copyable_v<From>, "BitCast requires trivially copyable types");
    union {
        From from;
        To to;
    } u{value};
    return u.to;
#endif
}

}  // namespace orteaf::internal::detail

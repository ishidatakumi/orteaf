#pragma once

#include <bit>
#include <cstdint>
#include <type_traits>

#if defined(__CUDACC__)
#include <cuda_fp16.h>
#endif

namespace orteaf::internal {

#if defined(__CUDACC__)
#define ORTEAF_INTERNAL_FLOAT16_HD __host__ __device__
#else
#define ORTEAF_INTERNAL_FLOAT16_HD
#endif

namespace detail {

template <class To, class From>
ORTEAF_INTERNAL_FLOAT16_HD constexpr To BitCast(From value) {
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

// Convert IEEE-754 binary32 to binary16 bits (round-to-nearest-even).
ORTEAF_INTERNAL_FLOAT16_HD constexpr std::uint16_t Float32ToHalfBits(float value) {
    using UInt32 = std::uint32_t;

    const UInt32 bits = BitCast<UInt32>(value);
    const UInt32 sign = (bits >> 16) & 0x8000u;
    UInt32 mantissa = bits & 0x007fffffu;
    int exponent = static_cast<int>((bits >> 23) & 0xffu);

    if (exponent == 255) {
        // Inf / NaN
        if (mantissa == 0) {
            return static_cast<std::uint16_t>(sign | 0x7c00u);
        }
        std::uint16_t nan = static_cast<std::uint16_t>(sign | 0x7c00u | (mantissa >> 13));
        return static_cast<std::uint16_t>(nan | ((mantissa & 0x1fffu) ? 1u : 0u));
    }

    int half_exponent = exponent - 127 + 15;

    if (half_exponent <= 0) {
        if (half_exponent < -10) {
            // Too small -> signed zero
            return static_cast<std::uint16_t>(sign);
        }

        // Subnormal
        mantissa |= 0x00800000u;
        const unsigned shift = static_cast<unsigned>(1 - half_exponent);
        std::uint32_t mant = mantissa >> shift;
        const std::uint32_t mask = (1u << shift) - 1u;
        const std::uint32_t remainder = mantissa & mask;
        const std::uint32_t halfway = 1u << (shift - 1);
        if ((remainder > halfway) || (remainder == halfway && (mant & 1u))) {
            ++mant;
        }
        return static_cast<std::uint16_t>(sign | mant);
    }

    if (half_exponent >= 31) {
        // Overflow -> Inf
        return static_cast<std::uint16_t>(sign | 0x7c00u);
    }

    mantissa += 0x00001000u;  // round to nearest even
    if (mantissa & 0x00800000u) {
        mantissa = 0;
        ++half_exponent;
        if (half_exponent >= 31) {
            return static_cast<std::uint16_t>(sign | 0x7c00u);
        }
    }

    return static_cast<std::uint16_t>(sign |
                                      (static_cast<std::uint16_t>(half_exponent) << 10) |
                                      (mantissa >> 13));
}

// Convert IEEE-754 binary16 bits to binary32.
ORTEAF_INTERNAL_FLOAT16_HD constexpr float HalfBitsToFloat32(std::uint16_t bits) {
    using UInt32 = std::uint32_t;

    const UInt32 sign = static_cast<UInt32>(bits & 0x8000u) << 16;
    UInt32 exponent = (bits >> 10) & 0x1fu;
    UInt32 mantissa = bits & 0x03ffu;

    if (exponent == 0) {
        if (mantissa == 0) {
            return BitCast<float>(sign);
        }
        // Subnormal
        int e = -14;
        while ((mantissa & 0x0400u) == 0u) {
            mantissa <<= 1;
            --e;
        }
        mantissa &= 0x03ffu;
        const UInt32 exp32 = static_cast<UInt32>(e + 127);
        const UInt32 mant32 = mantissa << 13;
        const UInt32 result = sign | (exp32 << 23) | mant32;
        return BitCast<float>(result);
    }

    if (exponent == 0x1fu) {
        // Inf / NaN
        UInt32 result = sign | 0x7f800000u | (mantissa << 13);
        if (mantissa != 0) {
            result |= 0x1u;  // ensure qNaN
        }
        return BitCast<float>(result);
    }

    const UInt32 exp32 = static_cast<UInt32>(exponent + (127 - 15));
    const UInt32 mant32 = mantissa << 13;
    const UInt32 result = sign | (exp32 << 23) | mant32;
    return BitCast<float>(result);
}

#if defined(__CUDACC__)
ORTEAF_INTERNAL_FLOAT16_HD inline std::uint16_t CudaHalfToBits(__half value) {
    return __half_as_ushort(value);
}

ORTEAF_INTERNAL_FLOAT16_HD inline __half BitsToCudaHalf(std::uint16_t bits) {
    return __ushort_as_half(bits);
}
#endif

}  // namespace detail

struct Float16 {
    std::uint16_t storage{};

    ORTEAF_INTERNAL_FLOAT16_HD constexpr Float16() = default;
    ORTEAF_INTERNAL_FLOAT16_HD explicit constexpr Float16(std::uint16_t bits) : storage(bits) {}

    ORTEAF_INTERNAL_FLOAT16_HD static constexpr Float16 FromBits(std::uint16_t bits) {
        return Float16(bits);
    }

    ORTEAF_INTERNAL_FLOAT16_HD constexpr std::uint16_t Bits() const { return storage; }

    ORTEAF_INTERNAL_FLOAT16_HD explicit constexpr Float16(float value)
        : storage(detail::Float32ToHalfBits(value)) {}

    ORTEAF_INTERNAL_FLOAT16_HD explicit constexpr Float16(double value)
        : storage(detail::Float32ToHalfBits(static_cast<float>(value))) {}

    ORTEAF_INTERNAL_FLOAT16_HD constexpr float ToFloat32() const {
        return detail::HalfBitsToFloat32(storage);
    }

    ORTEAF_INTERNAL_FLOAT16_HD constexpr double ToFloat64() const {
        return static_cast<double>(ToFloat32());
    }

#if defined(__FLT16_MANT_DIG__)
    ORTEAF_INTERNAL_FLOAT16_HD explicit constexpr Float16(_Float16 value)
        : storage(detail::Float32ToHalfBits(static_cast<float>(value))) {}

    ORTEAF_INTERNAL_FLOAT16_HD constexpr _Float16 ToNativeFloat16() const {
        return static_cast<_Float16>(ToFloat32());
    }
#endif

#if defined(__CUDACC__)
    ORTEAF_INTERNAL_FLOAT16_HD explicit Float16(__half value)
        : storage(detail::CudaHalfToBits(value)) {}

    ORTEAF_INTERNAL_FLOAT16_HD __half ToCudaHalf() const {
        return detail::BitsToCudaHalf(storage);
    }
#endif

    ORTEAF_INTERNAL_FLOAT16_HD friend constexpr bool operator==(Float16 lhs, Float16 rhs) {
        return lhs.storage == rhs.storage;
    }

    ORTEAF_INTERNAL_FLOAT16_HD friend constexpr bool operator!=(Float16 lhs, Float16 rhs) {
        return !(lhs == rhs);
    }
};

#undef ORTEAF_INTERNAL_FLOAT16_HD

static_assert(sizeof(Float16) == 2, "Float16 storage must be 16 bits");
static_assert(alignof(Float16) == alignof(std::uint16_t),
              "Float16 alignment should match 16-bit storage");
static_assert(std::is_trivially_copyable_v<Float16>, "Float16 must be trivially copyable");

}  // namespace orteaf::internal

#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>

#include "detail/bit_cast.h"

namespace orteaf::internal {

#if defined(__CUDACC__)
#define ORTEAF_INTERNAL_FLOAT8_HD __host__ __device__
#else
#define ORTEAF_INTERNAL_FLOAT8_HD
#endif

namespace detail {

constexpr float pow2(int exp) {
    float value = 1.0f;
    if (exp > 0) {
        for (int i = 0; i < exp; ++i) {
            value *= 2.0f;
        }
    } else if (exp < 0) {
        for (int i = 0; i < -exp; ++i) {
            value *= 0.5f;
        }
    }
    return value;
}

struct Fp8FormatSpec {
    int exponent_bits;
    int mantissa_bits;
    int exponent_bias;
    bool has_infinity;
    bool saturate_for_nan_inf;  // For formats without infinity (E4M3)

    constexpr std::uint8_t exponentMask() const {
        return static_cast<std::uint8_t>((1u << exponent_bits) - 1u);
    }

    constexpr std::uint8_t mantissaMask() const {
        return static_cast<std::uint8_t>((1u << mantissa_bits) - 1u);
    }

    constexpr std::uint8_t maxFiniteExponentField() const {
        const std::uint8_t mask = exponentMask();
        return has_infinity ? static_cast<std::uint8_t>(mask - 1u) : mask;
    }

    constexpr std::uint8_t maxFiniteBits() const {
        return static_cast<std::uint8_t>((maxFiniteExponentField() << mantissa_bits) | mantissaMask());
    }

    constexpr std::uint8_t infinityBits() const {
        return static_cast<std::uint8_t>(exponentMask() << mantissa_bits);
    }

    constexpr std::uint8_t quietNaNBits() const {
        return static_cast<std::uint8_t>((exponentMask() << mantissa_bits) | 0x1u);
    }
};

constexpr Fp8FormatSpec kFormatE4M3{4, 3, 7, false, true};
constexpr Fp8FormatSpec kFormatE5M2{5, 2, 15, true, false};

constexpr std::uint8_t packFp8Bits(std::uint8_t sign, std::uint8_t exponent_field, std::uint8_t mantissa_field,
                                   const Fp8FormatSpec& spec) {
    return static_cast<std::uint8_t>((sign << 7) | (exponent_field << spec.mantissa_bits) | mantissa_field);
}

constexpr std::uint8_t float32ToFp8(float value, const Fp8FormatSpec& spec) {
    const std::uint32_t bits = detail::bitCast<std::uint32_t>(value);
    const std::uint8_t sign = static_cast<std::uint8_t>(bits >> 31);
    const std::uint32_t exponent = (bits >> 23) & 0xffu;
    const std::uint32_t mantissa = bits & 0x7fffffu;
    const std::uint8_t sign_bits = static_cast<std::uint8_t>(sign << 7);

    if (exponent == 0xffu) {
        if (mantissa == 0) {
            if (spec.has_infinity) {
                return static_cast<std::uint8_t>(sign_bits | spec.infinityBits());
            }
            return static_cast<std::uint8_t>(sign_bits | spec.maxFiniteBits());
        }
        return static_cast<std::uint8_t>(sign_bits | spec.quietNaNBits());
    }

    if ((bits & 0x7fffffffU) == 0u) {
        return sign_bits;
    }

    const int max_finite_exponent = spec.maxFiniteExponentField();
    const std::uint32_t mantissa_full = mantissa | 0x00800000u;
    int exponent_field = static_cast<int>(exponent) - 127 + spec.exponent_bias;

    if (exponent_field > max_finite_exponent) {
        if (spec.has_infinity) {
            return static_cast<std::uint8_t>(sign_bits | spec.infinityBits());
        }
        return static_cast<std::uint8_t>(sign_bits | spec.maxFiniteBits());
    }

    const unsigned mantissa_shift = static_cast<unsigned>(23 - spec.mantissa_bits);

    if (exponent_field <= 0) {
        const unsigned shift = static_cast<unsigned>(1 - exponent_field) + mantissa_shift;
        if (shift >= 32) {
            return sign_bits;
        }
        std::uint32_t mant = mantissa_full >> shift;
        const std::uint32_t remainder_mask = (1u << shift) - 1u;
        const std::uint32_t remainder = mantissa_full & remainder_mask;
        const std::uint32_t halfway = 1u << (shift - 1);
        if ((remainder > halfway) || (remainder == halfway && (mant & 1u))) {
            ++mant;
        }
        if (mant == (1u << spec.mantissa_bits)) {
            return packFp8Bits(sign, 1u, 0u, spec);
        }
        return packFp8Bits(sign, 0u, static_cast<std::uint8_t>(mant & spec.mantissaMask()), spec);
    }

    std::uint32_t mant = mantissa_full >> mantissa_shift;
    const std::uint32_t remainder_mask = (1u << mantissa_shift) - 1u;
    const std::uint32_t remainder = mantissa_full & remainder_mask;
    const std::uint32_t halfway = 1u << (mantissa_shift - 1);
    if ((remainder > halfway) || (remainder == halfway && (mant & 1u))) {
        ++mant;
    }

    if (mant == (1u << (spec.mantissa_bits + 1))) {
        mant >>= 1;
        ++exponent_field;
        if (exponent_field > max_finite_exponent) {
            if (spec.has_infinity) {
                return static_cast<std::uint8_t>(sign_bits | spec.infinityBits());
            }
            return static_cast<std::uint8_t>(sign_bits | spec.maxFiniteBits());
        }
    }

    const std::uint8_t mantissa_field = static_cast<std::uint8_t>(mant & spec.mantissaMask());
    return packFp8Bits(sign, static_cast<std::uint8_t>(exponent_field), mantissa_field, spec);
}

constexpr float fp8ToFloat32(std::uint8_t storage, const Fp8FormatSpec& spec) {
    const std::uint8_t sign = storage >> 7;
    const std::uint8_t exponent_field = (storage >> spec.mantissa_bits) & spec.exponentMask();
    const std::uint8_t mantissa_field = storage & spec.mantissaMask();
    const float sign_value = sign ? -1.0f : 1.0f;

    if (exponent_field == spec.exponentMask()) {
        if (spec.has_infinity && mantissa_field == 0) {
            return sign ? -std::numeric_limits<float>::infinity() : std::numeric_limits<float>::infinity();
        }
        return std::numeric_limits<float>::quiet_NaN();
    }

    if (exponent_field == 0) {
        if (mantissa_field == 0) {
            return sign ? -0.0f : 0.0f;
        }
        const float fraction = static_cast<float>(mantissa_field) /
                               static_cast<float>(1u << spec.mantissa_bits);
        const float magnitude = fraction * pow2(1 - spec.exponent_bias);
        return sign_value * magnitude;
    }

    const float fraction = 1.0f + static_cast<float>(mantissa_field) /
                                      static_cast<float>(1u << spec.mantissa_bits);
    const int exponent = exponent_field - spec.exponent_bias;
    return sign_value * fraction * pow2(exponent);
}

}  // namespace detail

struct Float8E4M3 {
    std::uint8_t storage{};

    ORTEAF_INTERNAL_FLOAT8_HD constexpr Float8E4M3() = default;
    explicit ORTEAF_INTERNAL_FLOAT8_HD constexpr Float8E4M3(std::uint8_t bits) : storage(bits) {}
    explicit ORTEAF_INTERNAL_FLOAT8_HD constexpr Float8E4M3(float value)
        : storage(detail::float32ToFp8(value, detail::kFormatE4M3)) {}
    explicit ORTEAF_INTERNAL_FLOAT8_HD constexpr Float8E4M3(double value)
        : storage(detail::float32ToFp8(static_cast<float>(value), detail::kFormatE4M3)) {}

    static ORTEAF_INTERNAL_FLOAT8_HD constexpr Float8E4M3 fromBits(std::uint8_t bits) {
        return Float8E4M3(bits);
    }

    ORTEAF_INTERNAL_FLOAT8_HD constexpr std::uint8_t bits() const { return storage; }

    ORTEAF_INTERNAL_FLOAT8_HD constexpr float toFloat32() const {
        return detail::fp8ToFloat32(storage, detail::kFormatE4M3);
    }

    ORTEAF_INTERNAL_FLOAT8_HD constexpr double toFloat64() const {
        return static_cast<double>(toFloat32());
    }

    ORTEAF_INTERNAL_FLOAT8_HD friend constexpr bool operator==(Float8E4M3 lhs, Float8E4M3 rhs) {
        return lhs.storage == rhs.storage;
    }

    ORTEAF_INTERNAL_FLOAT8_HD friend constexpr bool operator!=(Float8E4M3 lhs, Float8E4M3 rhs) {
        return !(lhs == rhs);
    }
};

struct Float8E5M2 {
    std::uint8_t storage{};

    ORTEAF_INTERNAL_FLOAT8_HD constexpr Float8E5M2() = default;
    explicit ORTEAF_INTERNAL_FLOAT8_HD constexpr Float8E5M2(std::uint8_t bits) : storage(bits) {}
    explicit ORTEAF_INTERNAL_FLOAT8_HD constexpr Float8E5M2(float value)
        : storage(detail::float32ToFp8(value, detail::kFormatE5M2)) {}
    explicit ORTEAF_INTERNAL_FLOAT8_HD constexpr Float8E5M2(double value)
        : storage(detail::float32ToFp8(static_cast<float>(value), detail::kFormatE5M2)) {}

    static ORTEAF_INTERNAL_FLOAT8_HD constexpr Float8E5M2 fromBits(std::uint8_t bits) {
        return Float8E5M2(bits);
    }

    ORTEAF_INTERNAL_FLOAT8_HD constexpr std::uint8_t bits() const { return storage; }

    ORTEAF_INTERNAL_FLOAT8_HD constexpr float toFloat32() const {
        return detail::fp8ToFloat32(storage, detail::kFormatE5M2);
    }

    ORTEAF_INTERNAL_FLOAT8_HD constexpr double toFloat64() const {
        return static_cast<double>(toFloat32());
    }

    ORTEAF_INTERNAL_FLOAT8_HD friend constexpr bool operator==(Float8E5M2 lhs, Float8E5M2 rhs) {
        return lhs.storage == rhs.storage;
    }

    ORTEAF_INTERNAL_FLOAT8_HD friend constexpr bool operator!=(Float8E5M2 lhs, Float8E5M2 rhs) {
        return !(lhs == rhs);
    }
};

#undef ORTEAF_INTERNAL_FLOAT8_HD

static_assert(sizeof(Float8E4M3) == 1, "Float8E4M3 must occupy 1 byte");
static_assert(sizeof(Float8E5M2) == 1, "Float8E5M2 must occupy 1 byte");
static_assert(alignof(Float8E4M3) == alignof(std::uint8_t), "Float8E4M3 alignment");
static_assert(alignof(Float8E5M2) == alignof(std::uint8_t), "Float8E5M2 alignment");
static_assert(std::is_trivially_copyable_v<Float8E4M3>, "Float8E4M3 must be trivially copyable");
static_assert(std::is_trivially_copyable_v<Float8E5M2>, "Float8E5M2 must be trivially copyable");

}  // namespace orteaf::internal

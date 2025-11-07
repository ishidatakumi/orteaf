#pragma once

namespace orteaf::version {

inline constexpr int major() noexcept { return 0; }
inline constexpr int minor() noexcept { return 1; }
inline constexpr int patch() noexcept { return 0; }

inline constexpr const char* string() noexcept {
    return "0.1.0";
}

}  // namespace orteaf::version

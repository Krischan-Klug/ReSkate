#pragma once
#include <cstddef>
#include <string>

// How numbers show in the overlay, as skate. shows its stats: lengths in feet and speeds in miles
// per hour from the game's metres and metres per second, and thousands grouped. Plain functions.
namespace dingosdk::display_format {
inline constexpr float feet_per_metre = 3.28084f;
inline constexpr float mph_per_metre_per_second = 2.23694f;

constexpr float feet(float metres) noexcept { return metres * feet_per_metre; }
constexpr float mph(float metres_per_second) noexcept { return metres_per_second * mph_per_metre_per_second; }

// 12,345 and -1,234.
inline std::string grouped(long long value) {
    const bool negative = value < 0;
    const auto magnitude = negative ? 0ull - static_cast<unsigned long long>(value) : static_cast<unsigned long long>(value);
    auto digits = std::to_string(magnitude);
    for (auto at = static_cast<long long>(digits.size()) - 3; at > 0; at -= 3) digits.insert(static_cast<std::size_t>(at), ",");
    return negative ? "-" + digits : digits;
}
}

#pragma once
// A part of a decoded image (Engine/Resource/texture.h Image): cut out by fractions of its size,
// so the same part is named whatever size the image was decoded at.
#include "texture.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace dingosdk::frostbite {
// Fractions of an image's width and height: the whole image by default.
struct ImageRegion {
    float left{}, top{}, right{1}, bottom{1};
};

// The pixels of `region` of `image`, rounded to whole pixels and at least one. Throws when the
// region is not inside the image.
inline Image crop(const Image& image, const ImageRegion& region) {
    const auto [left, top, right, bottom] = region;
    if (!(left >= 0 && top >= 0 && right <= 1 && bottom <= 1 && left < right && top < bottom) || !image.width ||
        !image.height || image.rgba.size() != std::size_t{image.width} * image.height * 4)
        throw std::runtime_error("The region is not inside the image");
    const auto at = [](float fraction, std::uint32_t size) {
        return static_cast<std::uint32_t>(fraction * static_cast<float>(size) + 0.5f);
    };
    const auto x0 = std::min(at(left, image.width), image.width - 1), y0 = std::min(at(top, image.height), image.height - 1);
    const auto x1 = std::clamp(at(right, image.width), x0 + 1, image.width);
    const auto y1 = std::clamp(at(bottom, image.height), y0 + 1, image.height);
    Image result{x1 - x0, y1 - y0, std::vector<std::uint8_t>(std::size_t{x1 - x0} * (y1 - y0) * 4)};
    for (std::uint32_t y = y0; y < y1; ++y)
        std::memcpy(result.rgba.data() + std::size_t{y - y0} * result.width * 4,
                    image.rgba.data() + (std::size_t{y} * image.width + x0) * 4, std::size_t{result.width} * 4);
    return result;
}
}

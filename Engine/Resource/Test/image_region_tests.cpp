// A part of a decoded image, cut out by fractions of its size.
#include "Engine/Resource/image_region.h"
#include <iostream>

using namespace dingosdk::frostbite;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
// A width x height image whose every pixel holds its own x and y.
Image grid(std::uint32_t width, std::uint32_t height) {
    Image image{width, height, std::vector<std::uint8_t>(std::size_t{width} * height * 4)};
    for (std::uint32_t y = 0; y < height; ++y)
        for (std::uint32_t x = 0; x < width; ++x) {
            auto* p = image.rgba.data() + (std::size_t{y} * width + x) * 4;
            p[0] = static_cast<std::uint8_t>(x);
            p[1] = static_cast<std::uint8_t>(y);
            p[3] = 255;
        }
    return image;
}
bool pixel_is(const Image& image, std::uint32_t x, std::uint32_t y, std::uint8_t source_x, std::uint8_t source_y) {
    const auto* p = image.rgba.data() + (std::size_t{y} * image.width + x) * 4;
    return p[0] == source_x && p[1] == source_y;
}

void a_region_is_cut_out() {
    const auto image = grid(64, 32);
    const auto part = crop(image, {0.25f, 0.5f, 0.75f, 1.0f});
    check(part.width == 32 && part.height == 16 && part.rgba.size() == 32 * 16 * 4, "half of each side");
    check(pixel_is(part, 0, 0, 16, 16) && pixel_is(part, 31, 15, 47, 31), "the region's own pixels");
    const auto whole = crop(image, {});
    check(whole.width == 64 && whole.height == 32 && whole.rgba == image.rgba, "the default region is the whole image");
}

void the_same_region_at_any_size() {
    // The THRASHER wordmark's region of its 512 x 256 logo, at full size and at half.
    const ImageRegion wordmark{13.0f / 512, 39.0f / 256, 499.0f / 512, 193.0f / 256};
    const auto full = crop(grid(512, 256), wordmark), half = crop(grid(256, 128), wordmark);
    check(full.width == 486 && full.height == 154, "the wordmark at full size");
    check(half.width == 243 && half.height == 77, "and at half");
}

void a_region_keeps_at_least_a_pixel() {
    const auto part = crop(grid(4, 4), {0.5f, 0.5f, 0.51f, 0.51f});
    check(part.width == 1 && part.height == 1 && pixel_is(part, 0, 0, 2, 2), "a sliver is one pixel");
}

void a_region_outside_is_refused() {
    for (const auto& region : {ImageRegion{-0.1f, 0, 1, 1}, ImageRegion{0, 0, 1.1f, 1}, ImageRegion{0.5f, 0, 0.5f, 1},
                               ImageRegion{0, 0.8f, 1, 0.2f}}) {
        bool refused{};
        try { (void)crop(grid(8, 8), region); } catch (const std::runtime_error&) { refused = true; }
        check(refused, "a region not inside the image");
    }
}
}

int main() {
    try {
        a_region_is_cut_out();
        the_same_region_at_any_size();
        a_region_keeps_at_least_a_pixel();
        a_region_outside_is_refused();
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Image region tests passed.\n";
    return 0;
}

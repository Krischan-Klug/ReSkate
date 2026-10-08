#pragma once
#include <algorithm>
#include <array>
#include <cstddef>

// Where the parts of a 2D image go when the overlay draws it onto a box: a brush stroke whose bar
// is laid on the box with its splatter around it, or a panel nine-sliced to any size with its rough
// edges kept. Plain functions on boxes.
namespace dingosdk::image_layout {
// A rectangle: in pixels on the screen, or as fractions of an image (0 to 1).
struct Box {
    float left{}, top{}, right{1}, bottom{1};
};

// Where the whole image goes so that its `body` (fractions of it) covers `box`: the rest of it
// lies around the box.
constexpr Box around_body(Box box, Box body) noexcept {
    const float width = (box.right - box.left) / (body.right - body.left);
    const float height = (box.bottom - box.top) / (body.bottom - body.top);
    const float left = box.left - body.left * width, top = box.top - body.top * height;
    return {left, top, left + width, top + height};
}

// One of a nine-slice's parts: where it goes, and the part of the image (its texture coordinates)
// drawn there.
struct Patch {
    Box at, uv;
};
// The image's `uv` nine-sliced onto `box`: its corners `uv_edge_x` wide and `uv_edge_y` high, kept
// at `edge` pixels square, its edges stretched along them and its middle across the rest. A box
// smaller than two edges shrinks them to half of it. Rows top to bottom, each left to right.
inline std::array<Patch, 9> nine_slice(Box box, float edge, Box uv, float uv_edge_x, float uv_edge_y) noexcept {
    const float edge_x = std::clamp(edge, 0.0f, (box.right - box.left) * 0.5f);
    const float edge_y = std::clamp(edge, 0.0f, (box.bottom - box.top) * 0.5f);
    const std::array<float, 4> xs{box.left, box.left + edge_x, box.right - edge_x, box.right};
    const std::array<float, 4> ys{box.top, box.top + edge_y, box.bottom - edge_y, box.bottom};
    const std::array<float, 4> us{uv.left, uv.left + uv_edge_x, uv.right - uv_edge_x, uv.right};
    const std::array<float, 4> vs{uv.top, uv.top + uv_edge_y, uv.bottom - uv_edge_y, uv.bottom};
    std::array<Patch, 9> patches{};
    for (std::size_t row = 0; row < 3; ++row)
        for (std::size_t column = 0; column < 3; ++column)
            patches[row * 3 + column] = {{xs[column], ys[row], xs[column + 1], ys[row + 1]},
                                         {us[column], vs[row], us[column + 1], vs[row + 1]}};
    return patches;
}
}

// Where the parts of an image go on a box: around its body, or nine-sliced.
#include "Engine/Game/UI/image_layout.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace dingosdk::image_layout;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
bool near(float a, float b) { return std::abs(a - b) < 1e-4f; }
bool same(const Box& a, const Box& b) {
    return near(a.left, b.left) && near(a.top, b.top) && near(a.right, b.right) && near(a.bottom, b.bottom);
}

void the_body_covers_the_box() {
    check(same(around_body({10, 20, 110, 70}, {}), {10, 20, 110, 70}), "a whole image's body is the image");
    // A brush stroke whose bar is the middle half across and the middle third down.
    const Box stroke = around_body({100, 100, 300, 130}, {0.25f, 1.0f / 3, 0.75f, 2.0f / 3});
    check(same(stroke, {0, 70, 400, 160}), "its splatter lies around the box");
    // skate.'s bannerbrush_small (692 x 260): the bar is pixels 68 to 610 across, 79 to 184 down.
    const Box body{68.0f / 692, 79.0f / 260, 611.0f / 692, 185.0f / 260};
    const Box brush = around_body({0, 0, 543, 106}, body);
    check(same(brush, {-68, -79, 624, 181}), "at the size it was drawn, the bar on its own pixels");
}

void a_panel_keeps_its_corners() {
    const auto patches = nine_slice({0, 0, 300, 200}, 20, {0.5f, 0, 1, 0.5f}, 0.05f, 0.1f);
    check(same(patches[0].at, {0, 0, 20, 20}) && same(patches[0].uv, {0.5f, 0, 0.55f, 0.1f}), "the top left corner");
    check(same(patches[4].at, {20, 20, 280, 180}) && same(patches[4].uv, {0.55f, 0.1f, 0.95f, 0.4f}), "the middle stretched");
    check(same(patches[8].at, {280, 180, 300, 200}) && same(patches[8].uv, {0.95f, 0.4f, 1, 0.5f}), "the bottom right corner");
    check(same(patches[1].at, {20, 0, 280, 20}) && same(patches[3].at, {0, 20, 20, 180}), "the edges along them");
    float area{};
    for (const auto& patch : patches) area += (patch.at.right - patch.at.left) * (patch.at.bottom - patch.at.top);
    check(near(area, 300 * 200), "the patches cover the box once");
}

void a_small_box_shrinks_the_edges() {
    const auto patches = nine_slice({0, 0, 30, 100}, 20, {}, 0.1f, 0.1f);
    check(same(patches[0].at, {0, 0, 15, 20}) && same(patches[2].at, {15, 0, 30, 20}), "half the width each, no middle");
    check(near(patches[4].at.right - patches[4].at.left, 0) && near(patches[4].at.bottom - patches[4].at.top, 60),
        "the middle only down");
}
}

int main() {
    try {
        the_body_covers_the_box();
        a_panel_keeps_its_corners();
        a_small_box_shrinks_the_edges();
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Image layout tests passed.\n";
    return 0;
}

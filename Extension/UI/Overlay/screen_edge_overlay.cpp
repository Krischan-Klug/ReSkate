#include "overlay_internal.h"
#include <algorithm>

// The screen's edge as a hard hit shows it (overlay_internal.h draw_screen_edge): the picture
// darkened a little, the colour washing in from all four edges, top and bottom deeper than the
// sides. On the background draw list, under everything ReSkate draws, taking no input.

namespace dingosdk::overlay::detail {
namespace {
constexpr float darkening = 0.3f;     // the whole picture's, at full strength
constexpr float edge_opacity = 0.7f;  // the colour's, right at the edge
constexpr float top_bottom_reach = 0.3f, sides_reach = 0.18f; // of the screen's height and width
}

void draw_screen_edge(float strength, ImU32 colour) {
    strength = std::clamp(strength, 0.0f, 1.0f);
    const auto display = ImGui::GetIO().DisplaySize;
    if (strength <= 0 || display.x <= 0 || display.y <= 0) return;
    auto* draw = ImGui::GetBackgroundDrawList();
    const ImVec2 min(0, 0), max(display.x, display.y);
    draw->AddRectFilled(min, max, scaled_alpha(IM_COL32_BLACK, darkening * strength));
    const auto edge = scaled_alpha(colour, edge_opacity * strength), clear = scaled_alpha(colour, 0);
    const float vertical = display.y * top_bottom_reach * strength, horizontal = display.x * sides_reach * strength;
    // Each band from its edge (the colour) inwards (clear); corners: top left, top right, bottom right, bottom left.
    draw->AddRectFilledMultiColor(min, ImVec2(max.x, vertical), edge, edge, clear, clear);
    draw->AddRectFilledMultiColor(ImVec2(0, max.y - vertical), max, clear, clear, edge, edge);
    draw->AddRectFilledMultiColor(min, ImVec2(horizontal, max.y), edge, clear, clear, edge);
    draw->AddRectFilledMultiColor(ImVec2(max.x - horizontal, 0), max, clear, edge, edge, clear);
}
}

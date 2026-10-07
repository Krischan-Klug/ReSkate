#include "overlay_internal.h"
#include "Engine/Game/UI/display_format.h"
#include "Extension/UI/skate_theme.h"
#include <algorithm>
#include <cmath>
#include <vector>

// A score card (overlay.h ScoreCard) for any feature, laid out like skate. 3's Hall of Meat: a brush
// bar per stat with its icon, value and points, then a panel with the logo, the title and the total.
// Drawn with skate.'s own UI shapes (ScoreCardSkin) in its colours: dark bars and panel, scratched,
// its blue for the icons, the logo and the stroke under the total, white numbers. A shape not loaded draws a plain tile instead, or nothing. On the background draw list in
// the top right corner, taking no input.

namespace dingosdk::overlay::detail {
namespace {
namespace theme = dingosdk::skate_theme;
using display_format::grouped;
// From the screen's top right corner, in 1080p pixels; as wide as the logo it carries.
constexpr float corner_right = 48.0f, corner_top = 96.0f, card_width = 250.0f;
constexpr double row_fade_seconds = 0.3; // a new row opens and fades in this long

ImU32 with_alpha(ImU32 colour, float alpha) {
    const auto a = static_cast<unsigned>(((colour >> IM_COL32_A_SHIFT) & 0xff) * std::clamp(alpha, 0.0f, 1.0f));
    return (colour & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
}
void shadowed(ImDrawList* draw, ImFont* font, float size, ImVec2 at, ImU32 colour, const char* text) {
    const float offset = std::max(1.0f, size / 16.0f);
    const auto alpha = static_cast<float>((colour >> IM_COL32_A_SHIFT) & 0xff) / 255.0f;
    draw->AddText(font, size, ImVec2(at.x + offset, at.y + offset), with_alpha(theme::black, alpha * 0.7f), text);
    draw->AddText(font, size, at, colour, text);
}
float text_width(ImFont* font, float size, const std::string& text) {
    return font->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str()).x;
}

// A stat's row where it is drawn this frame, and how far it has faded in.
struct Line {
    const ScoreCardRow* row{};
    float top{}, shown{};
};
} // namespace

void draw_score_card(const ScoreCard& card, ScoreCardMotion& motion) {
    const auto display = ImGui::GetIO().DisplaySize;
    if (card.opacity <= 0 || display.x <= 0 || display.y <= 0) {
        motion = {};
        return;
    }
    // The total catches up with its value in about a quarter of a second.
    const double now = ImGui::GetTime();
    const float step = static_cast<float>(std::clamp(now - motion.at, 0.0, 0.1));
    motion.at = now;
    motion.total += (static_cast<float>(card.total) - motion.total) * (1.0f - std::exp(-step * 12.0f));
    if (std::abs(static_cast<float>(card.total) - motion.total) < 1.0f) motion.total = static_cast<float>(card.total);

    auto& s = state();
    auto* title_font = s.menu.title ? s.menu.title : ImGui::GetFont();
    auto* heading = s.menu.heading ? s.menu.heading : ImGui::GetFont();
    auto* bold = s.menu.bold ? s.menu.bold : ImGui::GetFont();
    auto* draw = ImGui::GetBackgroundDrawList();
    const float k = display.y / 1080.0f, o = card.opacity;
    const auto fade = [o](ImU32 colour) { return with_alpha(colour, o); };
    const float right = display.x - corner_right * k, left = right - card_width * k, pad = 14.0f * k;
    const auto& skin = card.skin;
    float y = corner_top * k;
    unsigned seed = 211u;

    // A bar per stat: its icon, what was measured and what it scored. A new one opens and fades in.
    // Every bar is drawn before any row's content, so no bar's splatter covers another's numbers.
    const float row_height = 30.0f * k, row_gap = 10.0f * k, icon = 22.0f * k, text = 18.0f * k;
    std::vector<Line> lines;
    for (const auto& row : card.rows) {
        const auto came = motion.since.try_emplace(row.key, now).first->second;
        const float shown = static_cast<float>(std::clamp((now - came) / row_fade_seconds, 0.0, 1.0));
        lines.push_back({&row, y, shown});
        y += (row_height + row_gap) * shown * (2.0f - shown); // eases out
    }
    for (const auto& line : lines) {
        const ImVec2 min(left, line.top), max(right, line.top + row_height);
        const auto colour = with_alpha(theme::tile, o * line.shown * 0.92f);
        if (!draw_game_shape(draw, skin.row, min, max, colour)) theme::rough_rect(draw, min, max, colour, ++seed, k);
    }
    for (const auto& line : lines) {
        const auto& row = *line.row;
        const float middle = line.top + row_height * 0.5f;
        const auto row_fade = [&](ImU32 colour) { return with_alpha(colour, o * line.shown); };
        float x = left + pad;
        if (!row.icon.empty() &&
            draw_game_image(draw, row.icon, ImVec2(x, middle - icon * 0.5f), ImVec2(x + icon, middle + icon * 0.5f),
                            row_fade(theme::blue)))
            x += icon + 10.0f * k;
        shadowed(draw, bold, text, ImVec2(x, middle - text * 0.5f), row_fade(theme::white), row.value.c_str());
        if (row.points) {
            const auto points = grouped(*row.points);
            shadowed(draw, bold, text, ImVec2(right - pad - text_width(bold, text, points), middle - text * 0.5f),
                     row_fade(theme::white), points.c_str());
        }
    }

    // The panel, scratched: the logo across it, the title, the total on a blue stroke and the badge, centred.
    const float logo_height = 72.0f * k, title_size = 22.0f * k, total_size = 56.0f * k, badge_size = 18.0f * k;
    const float edge = 16.0f * k;
    const float height = pad * 2.0f + (card.logo.empty() ? 0.0f : logo_height + 4.0f * k) + title_size + total_size +
                         (card.badge.empty() ? 0.0f : badge_size + 4.0f * k);
    const ImVec2 min(left, y), max(right, y + height);
    const auto panel = fade(with_alpha(theme::tile, 0.92f));
    if (draw_game_panel(draw, skin.panel, min, max, edge, panel))
        draw_game_shape(draw, skin.scratches, ImVec2(min.x + edge, min.y + edge), ImVec2(max.x - edge, max.y - edge),
                        fade(with_alpha(theme::white, 0.5f)));
    else
        theme::rough_rect(draw, min, max, panel, ++seed, k);
    const float centre = (left + right) * 0.5f;
    const auto centred = [&](ImFont* font, float size, ImU32 colour, const std::string& line) {
        shadowed(draw, font, size, ImVec2(centre - text_width(font, size, line) * 0.5f, y), colour, line.c_str());
        y += size;
    };
    y += pad;
    if (!card.logo.empty()) {
        draw_game_image(draw, card.logo, ImVec2(left + pad, y), ImVec2(right - pad, y + logo_height), fade(theme::blue));
        y += logo_height + 4.0f * k;
    }
    centred(heading, title_size, fade(theme::white), card.title);
    const auto total = grouped(static_cast<long long>(motion.total + 0.5f));
    const float stroke = text_width(title_font, total_size, total) * 0.5f + 20.0f * k;
    draw_game_shape(draw, skin.underline, ImVec2(centre - stroke, y + total_size * 0.55f),
                    ImVec2(centre + stroke, y + total_size * 0.95f), fade(theme::blue));
    centred(title_font, total_size, fade(theme::white), total);
    if (!card.badge.empty()) {
        y += 4.0f * k;
        centred(bold, badge_size, fade(card.highlight ? theme::good : theme::grey_text), card.badge);
    }
}
} // namespace dingosdk::overlay::detail

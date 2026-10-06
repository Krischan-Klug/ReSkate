#include "overlay_internal.h"
#include "Extension/UI/skate_theme.h"
#include <algorithm>
#include <cmath>
#include <format>

// Hall of Meat (Extension/Skater/hall_of_meat.h): from the local skater's bail until they get
// up, the bones it hurt over the world and the bail's card, its Meat counting up. The game side
// hands over the posed skeleton mesh in world space with the live camera, drawn as the overlay's
// x-ray (skeleton_overlay.cpp): only the hurt bodies, in their injury's colour, as in skate. 3.
// The card is drawn in skate.'s menu style like the S.K.A.T.E. HUD. All of it on the background draw list, under
// ReSkate's own menus and chat, taking no input.

namespace dingosdk::overlay {
namespace {
std::atomic<MeatFrame (*)()> frame_feed{};
std::atomic<bool (*)()> enabled_feed{};
}
void set_hall_of_meat_hooks(HallOfMeatHooks hooks) noexcept {
    frame_feed.store(hooks.frame);
    enabled_feed.store(hooks.enabled);
}
bool hall_of_meat_available() noexcept { return enabled_feed.load() != nullptr; }
bool hall_of_meat_enabled() noexcept {
    const auto enabled = enabled_feed.load();
    return enabled && enabled();
}
} // namespace dingosdk::overlay

namespace dingosdk::overlay::detail {
namespace {
namespace theme = dingosdk::skate_theme;
constexpr ImU32 bruised = IM_COL32(255, 196, 36, 255),
    broken = IM_COL32(232, 32, 32, 255), dark_red = IM_COL32(150, 12, 12, 255), hot = IM_COL32(255, 255, 255, 255);

struct MeatState {
    MeatFrame frame;
    float shown_score{}; // the card's Meat eases towards the score
    double counted_at{};
};
MeatState& meat() {
    static MeatState value;
    return value;
}
ImU32 with_alpha(ImU32 colour, float alpha) {
    const auto a = static_cast<unsigned>(((colour >> IM_COL32_A_SHIFT) & 0xff) * std::clamp(alpha, 0.0f, 1.0f));
    return (colour & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
}
ImU32 mix(ImU32 from, ImU32 to, float amount) {
    const auto channel = [&](int shift) {
        const float a = static_cast<float>((from >> shift) & 0xff), b = static_cast<float>((to >> shift) & 0xff);
        return static_cast<int>(a + (b - a) * std::clamp(amount, 0.0f, 1.0f));
    };
    return IM_COL32(channel(IM_COL32_R_SHIFT), channel(IM_COL32_G_SHIFT), channel(IM_COL32_B_SHIFT),
        channel(IM_COL32_A_SHIFT));
}
// Bruised bones are yellow, broken ones a slowly pulsing red; a fresh hit flashes white-hot.
ImU32 bone_colour(MeatInjury injury, float flash, double time) {
    const ImU32 colour = injury == MeatInjury::broken
        ? mix(dark_red, broken, 0.5f + 0.5f * static_cast<float>(std::sin(time * 6.0))) : bruised;
    return mix(colour, hot, flash * 0.8f);
}
// 12,345
std::string grouped(int value) {
    auto digits = std::to_string(std::max(value, 0));
    for (auto at = static_cast<int>(digits.size()) - 3; at > 0; at -= 3) digits.insert(static_cast<std::size_t>(at), ",");
    return digits;
}
void shadowed(ImDrawList* draw, ImFont* font, float size, ImVec2 at, ImU32 colour, const char* text) {
    const float offset = std::max(1.0f, size / 16.0f);
    const auto alpha = static_cast<float>((colour >> IM_COL32_A_SHIFT) & 0xff) / 255.0f;
    draw->AddText(font, size, ImVec2(at.x + offset, at.y + offset), with_alpha(theme::black, alpha * 0.7f), text);
    draw->AddText(font, size, at, colour, text);
}

// The hurt bodies, near solid in their injury's colour; the rest are not drawn.
void draw_meat_skeleton(const MeatSkeleton& value) {
    SkeletonPaints paints{};
    const double time = ImGui::GetTime();
    for (std::size_t body = 0; body < skater_body::count; ++body)
        if (value.injuries[body] != MeatInjury::none)
            paints[body] = SkeletonPaint{bone_colour(value.injuries[body], value.flashes[body], time), 0.6f};
    draw_skeleton(value.frame, paints, value.alpha);
}

// The bail's card in the top right corner (1080p pixels from its edges): its Meat and what made
// it, as it adds up. The bones it hurt show on the skeleton.
constexpr float corner_right = 48.0f, corner_top = 96.0f;
void draw_card(const MeatTally& tally, int score, float k) {
    auto& s = state();
    auto* title = s.menu.title ? s.menu.title : ImGui::GetFont();
    auto* heading = s.menu.heading ? s.menu.heading : ImGui::GetFont();
    auto* bold = s.menu.bold ? s.menu.bold : ImGui::GetFont();
    auto* draw = ImGui::GetBackgroundDrawList();
    const auto fade = [&](ImU32 colour) { return with_alpha(colour, tally.card); };
    const float width = 340.0f * k, pad = 18.0f * k, row = 24.0f * k;
    constexpr int stats = 5;
    const float height = pad * 2.0f + 30.0f * k + 52.0f * k + row * stats;
    const float right = ImGui::GetIO().DisplaySize.x - corner_right * k;
    const ImVec2 min(right - width, corner_top * k), max(right, min.y + height);
    theme::rough_rect(draw, min, max, fade(with_alpha(theme::tile, 0.92f)), 117u, k);
    draw->AddRectFilled(min, ImVec2(min.x + 5.0f * k, max.y), fade(broken));
    float y = min.y + pad;
    shadowed(draw, heading, 22.0f * k, ImVec2(min.x + pad, y), fade(theme::grey_text), "HALL OF MEAT");
    // The map's best on the right of the title: this bail's own when it set it.
    if (tally.best > 0) {
        const auto best = tally.new_best ? std::string("NEW BEST") : "BEST " + grouped(tally.best);
        const float best_width = bold->CalcTextSizeA(18.0f * k, FLT_MAX, 0.0f, best.c_str()).x;
        shadowed(draw, bold, 18.0f * k, ImVec2(max.x - pad - best_width, y + 3.0f * k),
                 fade(tally.new_best ? theme::good : theme::grey_text), best.c_str());
    }
    y += 30.0f * k;
    const auto total = grouped(score);
    shadowed(draw, title, 44.0f * k, ImVec2(min.x + pad, y), fade(theme::white), total.c_str());
    const float total_width = title->CalcTextSizeA(44.0f * k, FLT_MAX, 0.0f, total.c_str()).x;
    shadowed(draw, heading, 20.0f * k, ImVec2(min.x + pad + total_width + 10.0f * k, y + 18.0f * k), fade(broken), "MEAT");
    y += 52.0f * k;
    const auto stat = [&](const char* label, const std::string& value) {
        shadowed(draw, bold, 18.0f * k, ImVec2(min.x + pad, y), fade(theme::grey_text), label);
        const float value_width = bold->CalcTextSizeA(18.0f * k, FLT_MAX, 0.0f, value.c_str()).x;
        shadowed(draw, bold, 18.0f * k, ImVec2(max.x - pad - value_width, y), fade(theme::white), value.c_str());
        y += row;
    };
    stat("Damage", grouped(tally.damage));
    stat("Impacts", std::to_string(tally.impacts));
    stat("Broken bones", std::to_string(tally.broken));
    stat("Road rash", std::format("{:.1f} m", tally.road_rash));
    stat("Airtime", std::format("{:.1f} s", tally.airtime));
}
}

bool hall_of_meat_pending() {
    auto& m = meat();
    m.frame = {};
    if (const auto feed = frame_feed.load()) {
        try { m.frame = feed(); } catch (...) { m.frame = {}; }
    }
    if (m.frame.tally.card <= 0) m.shown_score = 0;
    return !m.frame.skeleton.frame.positions.empty() || m.frame.tally.card > 0;
}

void draw_hall_of_meat() {
    auto& m = meat();
    const auto display = ImGui::GetIO().DisplaySize;
    if (display.x <= 0 || display.y <= 0) return;
    const float k = display.y / 1080.0f;
    draw_meat_skeleton(m.frame.skeleton);
    const auto& tally = m.frame.tally;
    if (tally.card <= 0) return;
    // The Meat catches up with the score in about a quarter of a second.
    const double now = ImGui::GetTime();
    const float step = static_cast<float>(std::clamp(now - m.counted_at, 0.0, 0.1));
    m.counted_at = now;
    m.shown_score += (static_cast<float>(tally.score) - m.shown_score) * (1.0f - std::exp(-step * 12.0f));
    if (std::abs(static_cast<float>(tally.score) - m.shown_score) < 1.0f) m.shown_score = static_cast<float>(tally.score);
    draw_card(tally, static_cast<int>(m.shown_score + 0.5f), k);
}
} // namespace dingosdk::overlay::detail

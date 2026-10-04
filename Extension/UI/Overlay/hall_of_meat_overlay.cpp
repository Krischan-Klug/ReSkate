#include "overlay_internal.h"
#include "Extension/UI/skate_theme.h"
#include <algorithm>
#include <cmath>

// Hall of Meat (Extension/Skater/hall_of_meat.h): the local skater's skeleton over the world
// while a bail lasts and a few seconds after, a Meat counter while it lasts, and the bail's
// card after it. The game side hands over the bones in world space with the live camera;
// they are projected here like the nametags. The counter and card are drawn in skate.'s
// menu style like the S.K.A.T.E. HUD. All of it on the background draw list, under
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
using Vec3 = std::array<float, 3>;
constexpr ImU32 intact = IM_COL32(236, 230, 214, 255), bruised = IM_COL32(255, 196, 36, 255),
    broken = IM_COL32(232, 32, 32, 255), dark_red = IM_COL32(150, 12, 12, 255), hot = IM_COL32(255, 255, 255, 255);

struct MeatState {
    MeatFrame frame;
    float shown_score{}; // the counter eases towards the score
    double counted_at{};
};
MeatState& meat() {
    static MeatState value;
    return value;
}
float dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
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
// Intact bones are bone white, bruised ones yellow, broken ones a slowly pulsing red; a
// fresh hit flashes white-hot.
ImU32 bone_colour(MeatInjury injury, float flash, float alpha, double time) {
    ImU32 colour = injury == MeatInjury::hit ? bruised : injury == MeatInjury::broken ? broken : intact;
    if (injury == MeatInjury::broken) colour = mix(dark_red, broken, 0.5f + 0.5f * static_cast<float>(std::sin(time * 6.0)));
    return with_alpha(mix(colour, hot, flash * 0.8f), alpha * (injury == MeatInjury::none ? 0.8f : 1.0f));
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

void draw_skeleton(const MeatSkeleton& value, ImVec2 display, float k) {
    if (value.bones.empty() || value.alpha <= 0 || !(value.vertical_fov > 1 && value.vertical_fov < 175)) return;
    const auto& m = value.camera;
    const Vec3 right{m[0], m[1], m[2]}, up{m[4], m[5], m[6]}, back{m[8], m[9], m[10]}, origin{m[12], m[13], m[14]};
    const float focal = display.y / (2.0f * std::tan(value.vertical_fov * 3.14159265f / 360.0f));
    const ImVec2 centre(display.x * 0.5f, display.y * 0.5f);
    const auto project = [&](const Vec3& point, ImVec2& at, float& depth) {
        const Vec3 delta{point[0] - origin[0], point[1] - origin[1], point[2] - origin[2]};
        depth = -dot(delta, back);
        if (depth <= 0.1f) return false;
        at = ImVec2(centre.x + dot(delta, right) * focal / depth, centre.y - dot(delta, up) * focal / depth);
        return true;
    };
    auto* draw = ImGui::GetBackgroundDrawList();
    const double time = ImGui::GetTime();
    const auto shadow = with_alpha(IM_COL32(0, 0, 0, 150), value.alpha);
    struct Line {
        ImVec2 from, to;
        ImU32 colour;
        float width;
    };
    std::vector<Line> lines;
    lines.reserve(value.bones.size());
    for (const auto& bone : value.bones) {
        Line line;
        float from_depth{}, to_depth{};
        if (!project(bone.from, line.from, from_depth) || !project(bone.to, line.to, to_depth)) continue;
        line.colour = bone_colour(bone.injury, bone.flash, value.alpha, time);
        line.width = (bone.injury == MeatInjury::none ? 2.5f : 3.5f) * k + bone.flash * 2.5f * k;
        lines.push_back(line);
    }
    // Outlines first, so every bone reads against bright and dark ground alike.
    for (const auto& line : lines) draw->AddLine(line.from, line.to, shadow, line.width + 2.0f * k);
    for (const auto& line : lines) {
        draw->AddLine(line.from, line.to, line.colour, line.width);
        draw->AddCircleFilled(line.from, line.width * 0.75f, line.colour, 12);
        draw->AddCircleFilled(line.to, line.width * 0.75f, line.colour, 12);
    }
    const auto& skull = value.skull;
    ImVec2 at;
    float depth{};
    if (skull.radius > 0 && project(skull.centre, at, depth)) {
        const float radius = skull.radius * focal / depth;
        const auto colour = bone_colour(skull.injury, skull.flash, value.alpha, time);
        const float width = (skull.injury == MeatInjury::none ? 2.5f : 3.5f) * k + skull.flash * 2.5f * k;
        draw->AddCircle(at, radius, shadow, 32, width + 2.0f * k);
        draw->AddCircle(at, radius, colour, 32, width);
    }
}

// The counter and the card share the top right corner (1080p pixels from its edges): the
// counter while the bail lasts, the card after it.
constexpr float corner_right = 48.0f, corner_top = 96.0f;

// While the bail lasts: MEAT and the score, counting up, on a plate.
void draw_counter(int score, float k) {
    auto& s = state();
    auto* heading = s.menu.heading ? s.menu.heading : ImGui::GetFont();
    auto* draw = ImGui::GetBackgroundDrawList();
    const float label_size = 16.0f * k, number_size = 28.0f * k;
    const auto label_extent = heading->CalcTextSizeA(label_size, FLT_MAX, 0.0f, "MEAT");
    const auto number = grouped(score);
    const auto number_extent = heading->CalcTextSizeA(number_size, FLT_MAX, 0.0f, number.c_str());
    const float pad_x = 18.0f * k, pad_y = 8.0f * k;
    const float width = std::max(label_extent.x, number_extent.x) + pad_x * 2.0f;
    const float right = ImGui::GetIO().DisplaySize.x - corner_right * k;
    const ImVec2 min(right - width, corner_top * k);
    const ImVec2 max(right, min.y + pad_y * 2.0f + label_extent.y + number_extent.y);
    theme::rough_rect(draw, min, max, with_alpha(theme::tile, 0.9f), 93u, k);
    draw->AddRectFilled(min, ImVec2(min.x + 5.0f * k, max.y), broken);
    shadowed(draw, heading, label_size, ImVec2(max.x - pad_x - label_extent.x, min.y + pad_y), theme::grey_text, "MEAT");
    shadowed(draw, heading, number_size, ImVec2(max.x - pad_x - number_extent.x, min.y + pad_y + label_extent.y),
             theme::white, number.c_str());
}

// After the bail: its card, with the score and what made it. The bones it hurt show on
// the skeleton.
void draw_card(const MeatTally& tally, int score, float k) {
    auto& s = state();
    auto* title = s.menu.title ? s.menu.title : ImGui::GetFont();
    auto* heading = s.menu.heading ? s.menu.heading : ImGui::GetFont();
    auto* bold = s.menu.bold ? s.menu.bold : ImGui::GetFont();
    auto* draw = ImGui::GetBackgroundDrawList();
    const auto fade = [&](ImU32 colour) { return with_alpha(colour, tally.card); };
    const float width = 340.0f * k, pad = 18.0f * k, row = 24.0f * k;
    constexpr int stats = 3;
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
}
}

bool hall_of_meat_pending() {
    auto& m = meat();
    m.frame = {};
    if (const auto feed = frame_feed.load()) {
        try { m.frame = feed(); } catch (...) { m.frame = {}; }
    }
    const auto& tally = m.frame.tally;
    if (!tally.live && tally.card <= 0) m.shown_score = 0;
    return !m.frame.skeleton.bones.empty() || tally.live || tally.card > 0;
}

void draw_hall_of_meat() {
    auto& m = meat();
    const auto display = ImGui::GetIO().DisplaySize;
    if (display.x <= 0 || display.y <= 0) return;
    const float k = display.y / 1080.0f;
    draw_skeleton(m.frame.skeleton, display, k);
    const auto& tally = m.frame.tally;
    if (!tally.live && tally.card <= 0) return;
    // The counter catches up with the score in about a quarter of a second.
    const double now = ImGui::GetTime();
    const float step = static_cast<float>(std::clamp(now - m.counted_at, 0.0, 0.1));
    m.counted_at = now;
    m.shown_score += (static_cast<float>(tally.score) - m.shown_score) * (1.0f - std::exp(-step * 12.0f));
    if (std::abs(static_cast<float>(tally.score) - m.shown_score) < 1.0f) m.shown_score = static_cast<float>(tally.score);
    const int shown = static_cast<int>(m.shown_score + 0.5f);
    if (tally.live) draw_counter(shown, k);
    else draw_card(tally, shown, k);
}
} // namespace dingosdk::overlay::detail

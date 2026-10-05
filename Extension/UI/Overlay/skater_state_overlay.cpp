#include "overlay_internal.h"
#include "Extension/UI/skate_theme.h"
#include <algorithm>

// The skater state debug panel (Extension/Skater/skater_state_debug.h): the local skater's
// live state as one list in the bottom right corner, drawn in skate.'s menu style like the
// Hall of Meat card, on the background draw list, taking no input.

namespace dingosdk::overlay {
namespace {
std::atomic<std::vector<DebugField> (*)()> fields_feed{};
std::atomic<bool (*)()> enabled_feed{};
}
void set_skater_state_debug_hooks(SkaterStateDebugHooks hooks) noexcept {
    fields_feed.store(hooks.fields);
    enabled_feed.store(hooks.enabled);
}
bool skater_state_debug_available() noexcept { return enabled_feed.load() != nullptr; }
bool skater_state_debug_enabled() noexcept {
    const auto enabled = enabled_feed.load();
    return enabled && enabled();
}
} // namespace dingosdk::overlay

namespace dingosdk::overlay::detail {
namespace {
namespace theme = dingosdk::skate_theme;
constexpr ImU32 accent = IM_COL32(255, 196, 36, 255);

std::vector<DebugField>& panel() { static std::vector<DebugField> value; return value; } // render thread only

ImU32 with_alpha(ImU32 colour, float alpha) {
    const auto a = static_cast<unsigned>(((colour >> IM_COL32_A_SHIFT) & 0xff) * std::clamp(alpha, 0.0f, 1.0f));
    return (colour & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
}
void shadowed(ImDrawList* draw, ImFont* font, float size, ImVec2 at, ImU32 colour, const char* text) {
    const float offset = std::max(1.0f, size / 16.0f);
    draw->AddText(font, size, ImVec2(at.x + offset, at.y + offset), with_alpha(theme::black, 0.7f), text);
    draw->AddText(font, size, at, colour, text);
}
}

bool skater_state_debug_pending() {
    auto& fields = panel();
    fields.clear();
    if (const auto feed = fields_feed.load()) {
        try { fields = feed(); } catch (...) { fields.clear(); }
    }
    return !fields.empty();
}

// One list under its section titles, each line lit up for a moment when its value changes.
// The width follows the labels only, so the panel stays put while the values change.
void draw_skater_state_debug() {
    const auto& fields = panel();
    const auto display = ImGui::GetIO().DisplaySize;
    if (fields.empty() || display.x <= 0 || display.y <= 0) return;
    const float k = display.y / 1080.0f;
    auto& s = state();
    auto* heading = s.menu.heading ? s.menu.heading : ImGui::GetFont();
    auto* bold = s.menu.bold ? s.menu.bold : ImGui::GetFont();
    auto* draw = ImGui::GetBackgroundDrawList();
    const float size = 14.0f * k, pad = 12.0f * k, gap = 12.0f * k, row = size + 5.0f * k, margin = 48.0f * k;
    const float value_width = bold->CalcTextSizeA(size, FLT_MAX, 0.0f, "000 GROUND_ANIMATION").x;
    float label_width = 0;
    for (const auto& field : fields)
        if (!field.heading) label_width = std::max(label_width, bold->CalcTextSizeA(size, FLT_MAX, 0.0f, field.label.c_str()).x);
    const float width = pad * 2.0f + label_width + gap + value_width;
    const float height = pad * 2.0f + row * static_cast<float>(fields.size());
    const ImVec2 max(display.x - margin, display.y - margin), min(max.x - width, max.y - height);
    theme::rough_rect(draw, min, max, with_alpha(theme::tile, 0.9f), 61u, k);
    draw->AddRectFilled(min, ImVec2(min.x + 4.0f * k, max.y), accent);
    float y = min.y + pad;
    for (const auto& field : fields) {
        if (field.heading) {
            shadowed(draw, heading, size, ImVec2(min.x + pad, y), theme::grey_text, field.label.c_str());
        } else {
            if (field.changed > 0)
                draw->AddRectFilled(ImVec2(min.x + pad * 0.5f, y - 1.0f * k), ImVec2(max.x - pad * 0.5f, y + row - 2.0f * k),
                    with_alpha(accent, 0.5f * field.changed), 3.0f * k);
            shadowed(draw, bold, size, ImVec2(min.x + pad, y), theme::grey_text, field.label.c_str());
            const bool quiet = field.value == "no" || field.value == "-";
            shadowed(draw, bold, size, ImVec2(min.x + pad + label_width + gap, y), quiet ? theme::grey_text : theme::white,
                field.value.c_str());
        }
        y += row;
    }
}
} // namespace dingosdk::overlay::detail

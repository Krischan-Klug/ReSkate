#include "overlay_internal.h"
#include "Extension/UI/skate_theme.h"
#include <algorithm>

// The debug panel (Extension/Debug/debug_panel.h): the shown source's live values as one list
// in the bottom right corner, drawn in skate.'s menu style like the Hall of Meat card, on the
// background draw list, taking no input. SETTINGS > INTERFACE picks the source.

namespace dingosdk::overlay {
namespace {
std::atomic<DebugPanel (*)()> panel_feed{};
std::atomic<std::vector<DebugSource> (*)()> sources_feed{};
std::atomic<std::string (*)()> selected_feed{};
}
void set_debug_panel_hooks(DebugPanelHooks hooks) noexcept {
    panel_feed.store(hooks.panel);
    sources_feed.store(hooks.sources);
    selected_feed.store(hooks.selected);
}
std::vector<DebugSource> debug_panel_sources() {
    const auto sources = sources_feed.load();
    return sources ? sources() : std::vector<DebugSource>{};
}
std::string debug_panel_selected() {
    const auto selected = selected_feed.load();
    return selected ? selected() : std::string{};
}
} // namespace dingosdk::overlay

namespace dingosdk::overlay::detail {
namespace {
namespace theme = dingosdk::skate_theme;
constexpr ImU32 accent = IM_COL32(255, 196, 36, 255);

// Render thread only: this frame's panel, and the widest value the shown source had so far.
struct Shown {
    DebugPanel panel;
    std::string widest_of; // the source title `widest` belongs to
    float widest{};
};
Shown& shown() { static Shown value; return value; }

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

bool debug_panel_pending() {
    auto& panel = shown().panel;
    panel = {};
    if (const auto feed = panel_feed.load()) {
        try { panel = feed(); } catch (...) { panel = {}; }
    }
    return !panel.fields.empty();
}

// The source's title, then its list under its section titles, each line lit up for a moment
// when its value changes. The value column only ever widens while one source shows, so the
// panel stays put while the values change.
void draw_debug_panel() {
    auto& s = shown();
    const auto& panel = s.panel;
    const auto display = ImGui::GetIO().DisplaySize;
    if (panel.fields.empty() || display.x <= 0 || display.y <= 0) return;
    const float k = display.y / 1080.0f;
    auto& o = state();
    auto* heading = o.menu.heading ? o.menu.heading : ImGui::GetFont();
    auto* bold = o.menu.bold ? o.menu.bold : ImGui::GetFont();
    auto* draw = ImGui::GetBackgroundDrawList();
    const float size = 14.0f * k, pad = 12.0f * k, gap = 12.0f * k, row = size + 5.0f * k, margin = 48.0f * k;
    if (s.widest_of != panel.title) {
        s.widest_of = panel.title;
        s.widest = 0;
    }
    float label_width = 0;
    for (const auto& field : panel.fields) {
        if (field.heading) continue;
        label_width = std::max(label_width, bold->CalcTextSizeA(size, FLT_MAX, 0.0f, field.label.c_str()).x);
        s.widest = std::max(s.widest, bold->CalcTextSizeA(size, FLT_MAX, 0.0f, field.value.c_str()).x);
    }
    const float title_width = heading->CalcTextSizeA(size, FLT_MAX, 0.0f, panel.title.c_str()).x;
    const float width = pad * 2.0f + std::max(label_width + gap + s.widest, title_width);
    const float height = pad * 2.0f + row * static_cast<float>(panel.fields.size() + 1);
    const ImVec2 max(display.x - margin, display.y - margin), min(max.x - width, max.y - height);
    theme::rough_rect(draw, min, max, with_alpha(theme::tile, 0.9f), 61u, k);
    draw->AddRectFilled(min, ImVec2(min.x + 4.0f * k, max.y), accent);
    float y = min.y + pad;
    shadowed(draw, heading, size, ImVec2(min.x + pad, y), accent, panel.title.c_str());
    y += row;
    for (const auto& field : panel.fields) {
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

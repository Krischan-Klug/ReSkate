#include "debug_tool_console.h"
#include "debug_tool.h"
#include "Engine/Game/UI/menu_scale.h"
#include "Extension/UI/Overlay/overlay_internal.h"
#include "Extension/UI/skate_theme.h"
#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <format>
#include <string>

namespace dingosdk::overlay {
namespace {
namespace skate = skate_theme;

float scale() {
    return std::clamp(detail::state().model.menu_scale, dingosdk::min_menu_scale, dingosdk::max_menu_scale);
}

std::string clock(double seconds) {
    const auto total = static_cast<int>(seconds);
    return std::format("{:02}:{:02}", total / 60, total % 60);
}

// The probe list, as it was in the console tab.
void draw_probes(float k, ImFont* bold, ImFont* body) {
    const auto probes = debug_tool::probes();
    std::string_view system;
    for (const auto& probe : probes) {
        if (probe.system != system) {
            system = probe.system;
            ImGui::Spacing();
            ImGui::PushFont(bold);
            std::string caption(system);
            for (auto& c : caption) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            ImGui::TextDisabled("%s", caption.c_str());
            ImGui::PopFont();
        }
        ImGui::PushID(probe.id.data(), probe.id.data() + probe.id.size());
        bool on = probe.enabled;
        ImGui::BeginDisabled(!probe.available);
        ImGui::PushFont(bold);
        if (ImGui::Checkbox(std::string(probe.title).c_str(), &on)) debug_tool::set_enabled(probe.id, on);
        ImGui::PopFont();
        ImGui::EndDisabled();
        if (!probe.status.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, skate::blue);
            ImGui::TextWrapped("%s", probe.status.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::PushFont(body);
        ImGui::PushStyleColor(ImGuiCol_Text, skate::grey_text);
        ImGui::TextWrapped("%s", probe.available ? std::string(probe.description).c_str()
                                                 : "Unavailable: its code differs in this game build.");
        ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::PopID();
    }
    (void)k;
}

}

void draw_debug_window() {
    auto& s = detail::state();
    if (!s.console_visible.load() || !s.console_layout_valid) return;
    const float k = scale();
    const auto P = [k](float value) { return value * k; };
    const auto* viewport = ImGui::GetMainViewport();
    const float width = P(380), gap = P(8);
    float x = s.console_position.x + s.console_size.x + gap;
    if (x + width > viewport->WorkPos.x + viewport->WorkSize.x) x = std::max(viewport->WorkPos.x, s.console_position.x - width - gap);
    ImGui::SetNextWindowPos(ImVec2(x, s.console_position.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, s.console_size.y), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(P(14), P(12)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(10, 10, 11, 245));
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
    if (ImGui::Begin("Debug###ReSkateDebugTool", nullptr, flags)) {
        ImGui::PushFont(s.menu.bold);
        ImGui::TextUnformatted("DEBUG");
        ImGui::PopFont();
        ImGui::PushStyleColor(ImGuiCol_Text, skate::grey_text);
        ImGui::TextWrapped("Probes write [probe] lines to the log; recordings go to the logs folder.");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // The step recording first: one big switch, its live numbers and the key.
        const auto info = debug_tool::recording_info();
        ImGui::PushStyleColor(ImGuiCol_Button, info.active ? skate::danger : skate::blue);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, info.active ? IM_COL32(255, 120, 120, 255) : skate::blue_hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, info.active ? IM_COL32(220, 70, 70, 255) : skate::blue_active);
        ImGui::PushFont(s.menu.bold);
        if (ImGui::Button(info.active ? "STOP RECORDING" : "START RECORDING", ImVec2(-1, P(44))))
            debug_tool::set_enabled(debug_tool::recording_probe, !info.active);
        ImGui::PopFont();
        ImGui::PopStyleColor(3);
        ImGui::PushStyleColor(ImGuiCol_Text, info.active ? skate::danger : skate::grey_text);
        if (info.active)
            ImGui::TextWrapped("%s", std::format("REC {}  {} steps  {:.0f} MB  part {}", clock(info.seconds), info.steps,
                info.megabytes, info.part).c_str());
        else
            ImGui::TextWrapped("Runs until you stop it. F9 starts and stops it without the console.");
        ImGui::PopStyleColor();
        if (ImGui::Button("All probes off", ImVec2(-1, 0))) debug_tool::disable_all();
        ImGui::Separator();

        if (ImGui::BeginChild("##debug-probes", ImVec2(0, 0))) draw_probes(k, s.menu.bold, s.menu.body);
        ImGui::EndChild();
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

bool debug_hud_pending() { return debug_tool::recording_available(); }

// The recording card, drawn like Hall of Meat's score card (hall_of_meat_overlay.cpp): skate.'s dark tile, white
// numbers over a soft shadow, its blue stroke, in 1080p pixels from a screen corner — top left here, so it never meets
// the Meat card in the bottom left. Always on screen: small while idle ("F9 = record"), the full card while recording.
void draw_debug_hud() {
    const auto display = ImGui::GetIO().DisplaySize;
    if (display.x <= 0 || display.y <= 0) return;
    auto& s = detail::state();
    auto* title_font = s.menu.title ? s.menu.title : ImGui::GetFont();
    auto* heading = s.menu.heading ? s.menu.heading : ImGui::GetFont();
    auto* bold = s.menu.bold ? s.menu.bold : ImGui::GetFont();
    auto* draw = ImGui::GetBackgroundDrawList();
    const float k = display.y / 1080.0f;
    const auto shadowed = [&](ImFont* font, float size, ImVec2 at, ImU32 colour, const std::string& text) {
        const float offset = std::max(1.0f, size / 16.0f);
        draw->AddText(font, size, ImVec2(at.x + offset, at.y + offset), IM_COL32(0, 0, 0, 178), text.c_str());
        draw->AddText(font, size, at, colour, text.c_str());
    };
    const auto width_of = [](ImFont* font, float size, const std::string& text) {
        return font->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str()).x;
    };
    constexpr float corner_left = 64.0f, corner_top = 64.0f, card_width = 250.0f;
    const float left = corner_left * k, right = left + card_width * k, pad = 14.0f * k, top = corner_top * k;
    const auto info = debug_tool::recording_info();
    const ImU32 tile = IM_COL32(26, 26, 26, 235);

    if (!info.active) {
        const float size = 18.0f * k, height = size + pad * 1.4f;
        const ImVec2 min(left, top), max(right, top + height);
        skate::rough_rect(draw, min, max, tile, 301u, k);
        const float middle = top + height * 0.5f;
        draw->AddCircle(ImVec2(left + pad + 6.0f * k, middle), 6.0f * k, skate::grey_text, 0, 2.0f * k);
        shadowed(bold, size, ImVec2(left + pad + 20.0f * k, middle - size * 0.5f), skate::grey_text, "F9  =  RECORD");
        return;
    }

    const float title_size = 22.0f * k, time_size = 56.0f * k, line_size = 18.0f * k, foot_size = 16.0f * k;
    const float height = pad * 2.0f + title_size + time_size + line_size * 2.0f + foot_size + 12.0f * k;
    const ImVec2 min(left, top), max(right, top + height);
    skate::rough_rect(draw, min, max, tile, 307u, k);
    float y = top + pad;
    const float centre = (left + right) * 0.5f;
    // Title with the blinking dot.
    const std::string title = "RECORDING";
    const float title_width = width_of(heading, title_size, title);
    const float dot = 7.0f * k;
    const float title_left = centre - (title_width + dot * 2.0f + 8.0f * k) * 0.5f;
    if (static_cast<int>(info.seconds * 2) % 2 == 0)
        draw->AddCircleFilled(ImVec2(title_left + dot, y + title_size * 0.5f), dot, skate::danger);
    shadowed(heading, title_size, ImVec2(title_left + dot * 2.0f + 8.0f * k, y), skate::white, title);
    y += title_size;
    // The time, big, on skate.'s blue stroke.
    const auto total = static_cast<int>(info.seconds);
    const auto time = std::format("{:02}:{:02}", total / 60, total % 60);
    const float stroke = width_of(title_font, time_size, time) * 0.5f + 20.0f * k;
    draw->AddRectFilled(ImVec2(centre - stroke, y + time_size * 0.62f), ImVec2(centre + stroke, y + time_size * 0.9f), skate::blue);
    shadowed(title_font, time_size, ImVec2(centre - width_of(title_font, time_size, time) * 0.5f, y), skate::white, time);
    y += time_size + 4.0f * k;
    const auto row = [&](const std::string& text, ImU32 colour, float size) {
        shadowed(bold, size, ImVec2(centre - width_of(bold, size, text) * 0.5f, y), colour, text);
        y += size + 2.0f * k;
    };
    row(std::format("{} steps", info.steps), skate::white, line_size);
    row(std::format("{:.0f} MB   part {}", info.megabytes, info.part), skate::white, line_size);
    row("F9  =  STOP", skate::grey_text, foot_size);
}
}

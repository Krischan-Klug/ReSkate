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

bool foreground_is_game() {
    DWORD process{};
    GetWindowThreadProcessId(GetForegroundWindow(), &process);
    return process == GetCurrentProcessId();
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

void draw_debug_hud() {
    // F9 toggles the step recording while the game has focus, console open or not.
    static bool f9_down = false;
    const bool down = (GetAsyncKeyState(VK_F9) & 0x8000) != 0 && foreground_is_game();
    if (down && !f9_down) {
        const auto active = debug_tool::recording_info().active;
        debug_tool::set_enabled(debug_tool::recording_probe, !active);
    }
    f9_down = down;

    const auto info = debug_tool::recording_info();
    if (!info.active) return;
    auto& s = detail::state();
    const float k = scale();
    const auto P = [k](float value) { return value * k; };
    auto* font = s.menu.bold ? s.menu.bold : ImGui::GetFont();
    auto* draw = ImGui::GetForegroundDrawList();
    const auto text = std::format("REC  {}   {} steps   {:.0f} MB   part {}    F9 = stop", clock(info.seconds), info.steps,
        info.megabytes, info.part);
    const float size = P(18);
    const auto extent = font->CalcTextSizeA(size, FLT_MAX, 0, text.c_str());
    const auto* viewport = ImGui::GetMainViewport();
    const float dot = P(8), pad = P(10);
    const ImVec2 box(extent.x + dot * 2 + pad * 3, extent.y + pad * 2);
    const ImVec2 at(viewport->WorkPos.x + (viewport->WorkSize.x - box.x) * 0.5f, viewport->WorkPos.y + P(10));
    draw->AddRectFilled(at, ImVec2(at.x + box.x, at.y + box.y), IM_COL32(10, 10, 11, 220));
    draw->AddRect(at, ImVec2(at.x + box.x, at.y + box.y), skate::danger, 0, 0, P(2));
    // The dot blinks once a second.
    if (static_cast<int>(info.seconds * 2) % 2 == 0)
        draw->AddCircleFilled(ImVec2(at.x + pad + dot, at.y + box.y * 0.5f), dot, skate::danger);
    draw->AddText(font, size, ImVec2(at.x + pad * 2 + dot * 2, at.y + pad), skate::white, text.c_str());
}
}

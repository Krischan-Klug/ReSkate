#include "debug_tool_console.h"
#include "debug_tool.h"
#include "Extension/UI/skate_theme.h"
#include <imgui.h>
#include <cctype>
#include <string>

namespace dingosdk::overlay {
void draw_debug_tool(float k, float height, ImFont* bold, ImFont* body) {
    namespace skate = skate_theme;
    const auto P = [k](float value) { return value * k; };
    auto* draw = ImGui::GetWindowDrawList();
    {
        const auto at = ImGui::GetCursorScreenPos();
        skate::rough_rect(draw, at, ImVec2(at.x + ImGui::GetContentRegionAvail().x, at.y + height), skate::tile, 33, k);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(P(14), P(12)));
    if (ImGui::BeginChild("Debug probes", ImVec2(0, height), ImGuiChildFlags_AlwaysUseWindowPadding)) {
        const auto probes = debug_tool::probes();
        ImGui::PushStyleColor(ImGuiCol_Text, skate::grey_text);
        ImGui::TextWrapped("Probes report what the game does. Their lines go to the LOG tab and ReSkate.log as "
                           "[probe] ... (category Diagnostics); recordings go to the logs folder.");
        ImGui::PopStyleColor();
        if (ImGui::Button("All off")) debug_tool::disable_all();
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
            ImGui::SameLine();
            ImGui::TextDisabled("%.*s", static_cast<int>(probe.id.size()), probe.id.data());
            if (!probe.status.empty()) {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, skate::blue);
                ImGui::TextUnformatted(probe.status.c_str());
                ImGui::PopStyleColor();
            }
            ImGui::PushFont(body);
            ImGui::PushStyleColor(ImGuiCol_Text, skate::grey_text);
            ImGui::Indent(ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::TextWrapped("%s", probe.available ? std::string(probe.description).c_str()
                                                     : "Unavailable: its code differs in this game build.");
            ImGui::Unindent(ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::PopStyleColor();
            ImGui::PopFont();
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
}
}

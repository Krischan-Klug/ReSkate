#include "hud_corner_debug.h"
#include "hud_corner.h"
#include "Extension/UI/NativeUi/model_takeover.h"
#include <format>
#include <string>

namespace dingosdk::hud_corner {
namespace {
using debug_panel::Field;

// "hallofmeat: all", "-" for nobody.
std::string covered_by() {
    std::string text;
    for (const auto& claim : claims()) text += std::format("{}{}: {}", text.empty() ? "" : ", ", claim.owner, name(claim.cover));
    return text.empty() ? "-" : text;
}
// "HUDDpad_Widget p1 active, ours p2", "empty".
std::string items(const State& state) {
    std::string text;
    for (const auto& item : state.items)
        text += std::format("{}{} p{}{}", text.empty() ? "" : ", ",
                            item.ours ? "ours" : item.widget.substr(item.widget.rfind('/') + 1), item.priority,
                            item.active ? " active" : "");
    return text.empty() ? "empty" : text;
}

std::vector<Field> sample() {
    const auto s = state();
    std::vector<Field> fields;
    fields.push_back({"HUD CORNER", {}, true});
    fields.push_back({"Covered by", covered_by()});
    fields.push_back({"Model takeover", native_ui::model_takeover_available() ? "in" : "unavailable"});
    fields.push_back({"D-PAD STACK", {}, true});
    fields.push_back({"Stack", s.stack_found ? std::format("target {}{}", s.target_index, s.stack_active ? ", active" : "") : "not found"});
    fields.push_back({"Items", s.stack_found ? items(s) : "-"});
    fields.push_back({"SCORE HUD", {}, true});
    fields.push_back({"Shown", s.score_found ? std::format("{} (style {}){}", debug_panel::yes_no(s.score_shown), s.extra_info_style,
                                                           s.score_held ? ", held down" : "") : "not found"});
    // The game writes the score HUD's fields every frame: the count runs, logged beside the other changes.
    fields.push_back({"Held back", std::to_string(native_ui::held_back()), false, true});
    return fields;
}
}

debug_panel::Source debug_source() { return {"hudcorner", "HUD corner", &sample}; }
}

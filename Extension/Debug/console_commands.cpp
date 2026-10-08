#include "Extension/Console/commands.h"
#include "debug_panel.h"
#include "Extension/UI/NativeHud/hud_corner.h"

namespace dingosdk::console {
void register_debug_commands(Commands &registry) {
    auto source = argument("source|off");
    source.complete = [](const Model &, auto) {
        std::vector<std::string> names{"off"};
        for (const auto &choice : debug_panel::sources())
            names.push_back(choice.id);
        return names;
    };
    auto panel = variable("debugpanel",
        "Debug panel: one source's live values in the bottom right corner, each change logged (not saved)",
        Group::console, source);
    panel.execution = Execution::local;
    panel.inspect = [](const Model &) {
        std::string ids;
        for (const auto &choice : debug_panel::sources())
            ids += (ids.empty() ? "" : ", ") + choice.id;
        const auto shown = debug_panel::selected();
        return State{true, shown.empty() ? std::string("off") : shown, {}, "Sources: " + ids, false};
    };
    panel.run = [](const Model &, const Values &args, const Output &out) {
        const auto id = lower(std::get<std::string>(args[0]));
        if (id == "off") {
            debug_panel::select({});
            out("Debug panel off.");
            return;
        }
        std::string ids;
        for (const auto &choice : debug_panel::sources()) {
            if (choice.id == id && debug_panel::select(id)) {
                out("Debug panel: " + choice.title + ".");
                return;
            }
            ids += ", " + choice.id;
        }
        out("error: source must be one of: off" + ids);
    };
    registry.add(std::move(panel));

    // Covering skate.'s bottom left HUD corner (Extension/UI/NativeHud/hud_corner.h) by hand, to try it without a feature.
    auto cover = argument("none|dpad|all");
    cover.choices = {"none", "dpad", "all"};
    auto corner = action("hudcorner", "Cover skate.'s bottom left HUD corner: dpad hides the d-pad, all the score HUD too, "
        "none gives it back (not saved)", Group::console, {cover});
    corner.execution = Execution::local;
    corner.run = [](const Model &, const Values &args, const Output &out) {
        const auto word = lower(std::get<std::string>(args[0]));
        const auto chosen = word == "all" ? hud_corner::Cover::all : word == "dpad" ? hud_corner::Cover::dpad : hud_corner::Cover::none;
        hud_corner::set_cover("console", chosen);
        out("HUD corner covered: " + std::string(hud_corner::name(chosen)) + ".");
    };
    registry.add(std::move(corner));
}
} // namespace dingosdk::console

#include "Extension/Console/commands.h"
#include "debug_panel.h"

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
}
} // namespace dingosdk::console

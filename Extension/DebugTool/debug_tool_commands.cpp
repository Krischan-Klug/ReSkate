#include "debug_tool.h"
#include "Extension/Console/commands.h"

// `debug` lists the probes, `debug <probe> 0|1` switches one, `debug off` all (debug_tool.h).
namespace dingosdk::console {
void register_debug_tool_commands(Commands &registry) {
    auto probe = argument("probe", Type::text, true);
    probe.complete = [](const Model &, auto) {
        std::vector<std::string> names{"off"};
        for (const auto &p : debug_tool::probes())
            names.emplace_back(p.id);
        return names;
    };
    auto debug = action("debug", "Debug probes for reverse engineering: list them, switch one on or off, or `debug off`",
                        Group::console, {probe, argument("0|1", Type::boolean, true)});
    debug.execution = Execution::local;
    debug.run = [](const Model &, const Values &args, const Output &out) {
        const auto id = args.empty() ? std::string{} : std::get<std::string>(args[0]);
        if (id == "off") {
            debug_tool::disable_all();
            out("All debug probes off.");
            return;
        }
        if (!id.empty()) {
            const bool on = args.size() < 2 || std::get<bool>(args[1]);
            out(debug_tool::set_enabled(id, on) ? id + (on ? " on" : " off")
                                                : "No probe " + id + " for this game build. `debug` lists them.");
            return;
        }
        for (const auto &p : debug_tool::probes())
            out(std::string(p.enabled ? "[on]  " : p.available ? "[off] " : "[n/a] ") + std::string(p.id) + "  " +
                std::string(p.description) + (p.status.empty() ? "" : "  (" + p.status + ")"));
    };
    registry.add(std::move(debug));
}
} // namespace dingosdk::console

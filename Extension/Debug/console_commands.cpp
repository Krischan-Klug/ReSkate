#include "Extension/Console/commands.h"
#include "debug_panel.h"
#include "write_watch.h"
#include "Extension/Skater/local_skater_body.h"
#include "Extension/UI/NativeHud/hud_corner.h"
#include <Windows.h>
#include <charconv>
#include <format>

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

    // Who writes a field, live (write_watch.h): a hardware watch on one address, each writer and each change logged.
    auto target = argument("skaterstep|off|0x<address>|ghidra:0x<address>");
    target.choices = {"skaterstep", "off"};
    auto bytes = argument("bytes", Type::unsigned_integer, true);
    bytes.choices = {"1", "2", "4", "8"};
    auto watch = action("debugwatch", "Watch who writes an address (a hardware watch; each writer and each change is "
        "logged): skaterstep = the local skater's physics step length; ghidra: takes an address as Ghidra shows it",
        Group::console, {target, bytes});
    watch.execution = Execution::local;
    watch.inspect = [](const Model &) {
        const auto s = write_watch::summary();
        return State{true, s.armed ? std::format("{} at {:#x}: {} writes, {} writer(s)", s.name, s.address, s.writes, s.writers)
                                   : std::string("off"), {}, {}, false};
    };
    watch.run = [](const Model &, const Values &args, const Output &out) {
        const auto word = lower(std::get<std::string>(args[0]));
        if (word == "off") {
            write_watch::disarm();
            out("Write watch off; the writers are in the log.");
            return;
        }
        std::string error;
        if (word == "skaterstep") {
            if (write_watch::arm("skater step length", [] { return skater_body::step_length_address(); }, 4, error))
                out("Watching the local skater's physics step length; writers and changes go to the log.");
            else
                out("error: " + error);
            return;
        }
        const bool ghidra = word.starts_with("ghidra:");
        auto text = std::string_view(word).substr(ghidra ? 7 : 0);
        if (text.starts_with("0x")) text.remove_prefix(2);
        std::uintptr_t address{};
        if (text.empty() || std::from_chars(text.data(), text.data() + text.size(), address, 16).ec != std::errc{}) {
            out("error: target must be skaterstep, off, 0x<address> or ghidra:0x<address>");
            return;
        }
        // Ghidra shows Skate.exe at its preferred base; the game runs wherever Windows put it.
        if (ghidra) address = address - 0x140000000 + reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        const auto size = args.size() > 1 ? static_cast<std::size_t>(std::get<std::uint64_t>(args[1])) : 4;
        if (write_watch::arm(std::format("{:#x}", address), [address] { return address; }, size, error))
            out(std::format("Watching {} bytes at {:#x}; writers and changes go to the log.", size, address));
        else
            out("error: " + error);
    };
    registry.add(std::move(watch));
}
} // namespace dingosdk::console

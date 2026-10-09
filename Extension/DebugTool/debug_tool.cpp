#include "debug_tool.h"
#include "debug_tool_internal.h"
#include <array>

namespace dingosdk::debug_tool {
namespace {
template<class Visit> void each_probe(Visit&& visit) {
    const std::array systems{skater_probes()};
    for (auto list : systems)
        for (auto& probe : list) visit(probe);
}
bool usable(const Probe& probe) { return !probe.available || probe.available(); }
}

bool start(std::uintptr_t image_base) noexcept {
    const bool skater = start_skater_probes(image_base);
    logging::log(logging::Level::info, logging::Channel::diagnostics, "Debug tool ready (skater probes {}); console: debug.",
        skater ? "on" : "unavailable");
    return skater;
}

std::vector<ProbeInfo> probes() {
    std::vector<ProbeInfo> result;
    each_probe([&](const Probe& probe) {
        const bool on = probe.enabled.load();
        result.push_back({probe.id, probe.system, probe.title, probe.description, usable(probe), on,
                          on && probe.status ? probe.status() : std::string{}});
    });
    return result;
}

bool set_enabled(std::string_view id, bool on) noexcept {
    bool found = false;
    each_probe([&](Probe& probe) {
        if (found || id != probe.id) return;
        found = true;
        if (!usable(probe)) {
            found = false;
            return;
        }
        if (probe.enabled.load() == on) return;
        if (on && probe.switched) probe.switched(true);
        probe.enabled.store(on);
        if (!on && probe.switched) probe.switched(false);
        report(probe, "{}", on ? "on" : "off");
    });
    return found;
}

void disable_all() noexcept {
    each_probe([](Probe& probe) {
        if (probe.enabled.load()) (void)set_enabled(probe.id, false);
    });
}
}

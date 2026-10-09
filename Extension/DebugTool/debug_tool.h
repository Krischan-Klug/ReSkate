#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// The debug tool: probes that report what skate. does, so ReSkate doubles as the instrument for reverse
// engineering the game (re/README.md). Every probe is off until switched on in the console's DEBUG tab or
// with `debug <probe> 1`. A probe writes its lines to ReSkate.log as "[<probe>] ..." (console category
// Diagnostics); bulk data goes to a file in logs/. A probe belongs to one system of re/scripts/systems.py.
namespace dingosdk::debug_tool {
struct ProbeInfo {
    std::string_view id, system, title, description;
    bool available{}, enabled{};
    std::string status; // a live line for the DEBUG tab, empty when there is nothing to say
};
// Installs the probes' hooks (each checks its code's bytes first); requires No Bail started.
bool start(std::uintptr_t image_base) noexcept;
std::vector<ProbeInfo> probes();
// False when there is no such probe or it is unavailable for this game build.
bool set_enabled(std::string_view id, bool on) noexcept;
void disable_all() noexcept;
}

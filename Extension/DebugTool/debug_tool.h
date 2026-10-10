#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// The debug tool: probes that report what skate. does, so ReSkate doubles as the instrument for reverse
// engineering the game (re/README.md). Every probe is off until switched on in the DEBUG window next to the console
// or with `debug <probe> 1`; F9 starts and stops the step recording without the console. A probe writes its lines to ReSkate.log as "[<probe>] ..." (console category
// Diagnostics); bulk data goes to a file in logs/. A probe belongs to one system of re/scripts/systems.py.
namespace dingosdk::debug_tool {
struct ProbeInfo {
    std::string_view id, system, title, description;
    bool available{}, enabled{};
    std::string status; // a live line for the DEBUG window, empty when there is nothing to say
};
// Installs the probes' hooks (each checks its code's bytes first); requires No Bail started.
bool start(std::uintptr_t image_base) noexcept;
std::vector<ProbeInfo> probes();
// False when there is no such probe or it is unavailable for this game build.
bool set_enabled(std::string_view id, bool on) noexcept;
void disable_all() noexcept;

// The step recording (probe skater.step): runs until stopped, in parts of a few hundred MB.
struct RecordingInfo {
    bool active{};
    double seconds{}, megabytes{};
    std::uint64_t steps{};
    int part{};
};
RecordingInfo recording_info() noexcept;
// True once the step probe's hooks are in (the recording can run).
bool recording_available() noexcept;
// Starts or stops the recording (F9); any thread.
void toggle_recording() noexcept;
inline constexpr std::string_view recording_probe = "skater.step";
}

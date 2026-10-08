#include "game_speed.h"
#include "named_settings.h"
#include "Engine/Game/Multiplayer/session_tools.h"
#include "Engine/Game/Settings/game_speed.h"
#include <charconv>
#include <format>
#include <string>

namespace dingosdk {
namespace {
// The game's own simulation rate, as the engine holds it now ("60").
std::optional<std::uint32_t> current_rate() {
    const auto text = read_named_setting("SimulationTime.ForceSimRate");
    std::uint32_t rate{};
    if (!text || std::from_chars(text->data(), text->data() + text->size(), rate).ec != std::errc{} || !rate) return std::nullopt;
    return rate;
}
}

bool GameSpeed::set(float speed) {
    if (!(speed < 1.0f) || multiplayer_session_active()) {
        release();
        return false;
    }
    if (!base_rate_) base_rate_ = current_rate();
    if (!base_rate_) return false;
    const auto rate = std::to_string(game_speed::simulation_rate(*base_rate_, speed));
    // The steps first, then the clock: no step of the change runs at the old rate on the new clock.
    if (sim_rate_.hold(rate) && max_sim_fps_.hold(rate) && time_scale_.hold(std::format("{:.3f}", speed))) return true;
    release(); // all three or none
    return false;
}

void GameSpeed::release() {
    time_scale_.release();
    max_sim_fps_.release();
    sim_rate_.release();
    base_rate_.reset();
}
}

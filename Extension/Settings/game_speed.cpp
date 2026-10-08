#include "game_speed.h"
#include "named_settings.h"
#include "Engine/Game/Multiplayer/session_tools.h"
#include "Engine/Game/Settings/game_speed.h"
#include "Extension/Skater/local_skater_body.h"
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
    const auto rate = game_speed::simulation_rate(*base_rate_, speed);
    const auto text = std::to_string(rate);
    // The steps first, then the clock: no step of the change runs at the old rate on the new clock.
    if (!sim_rate_.hold(text) || !max_sim_fps_.hold(text) || !time_scale_.hold(std::format("{:.3f}", speed))) {
        release(); // all three or none
        return false;
    }
    // The skater steps as the simulation now does (a skater rebuilt meanwhile already does).
    if (skater_body::set_step_length(game_speed::step_seconds(rate))) skater_changed_ = true;
    return true;
}

void GameSpeed::release() {
    time_scale_.release();
    max_sim_fps_.release();
    sim_rate_.release();
    // The skater's own step back; while no skater can be reached (a teleport under way), the next call
    // tries again, and the game's own rate is kept for it until then.
    if (skater_changed_ && base_rate_ && skater_body::set_step_length(game_speed::step_seconds(*base_rate_)))
        skater_changed_ = false;
    if (!skater_changed_) base_rate_.reset();
}
}

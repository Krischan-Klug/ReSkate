#include "hall_of_meat_slow_motion.h"
#include "hall_of_meat_skater.h"
#include "Engine/Game/Multiplayer/session_tools.h"
#include "Engine/Game/Settings/game_speed.h"
#include "Extension/Settings/named_settings.h"
#include <charconv>
#include <format>

namespace dingosdk::hall_of_meat {
namespace {
bool failed(const std::string& result) { return result.starts_with("error: "); }

// The game's own simulation rate, as the engine holds it now ("60").
std::optional<std::uint32_t> current_rate() {
    const auto text = read_named_setting("SimulationTime.ForceSimRate");
    std::uint32_t rate{};
    if (!text || std::from_chars(text->data(), text->data() + text->size(), rate).ec != std::errc{} || !rate) return std::nullopt;
    return rate;
}
}

// Holds the setting at `value`, keeping what it was the first time; false when it cannot be changed
// now (then nothing new is held), or once something else changed it while it was held.
bool SlowMotion::Held::hold(const std::string& value) {
    if (yielded) return false;
    const auto now = read_named_setting(name);
    if (!now) return false;
    if (before && *now != ours) { // changed by someone else: theirs now, ours no longer
        before.reset();
        yielded = true;
        return false;
    }
    if (failed(change_named_setting(name, value, false))) return false;
    const auto after = read_named_setting(name);
    if (!after) return false;
    if (!before) before = *now;
    ours = *after;
    return true;
}

// Gives the setting back, unless it is someone else's now.
void SlowMotion::Held::release() {
    yielded = false;
    if (!before) return;
    const auto now = read_named_setting(name);
    if (now && *now == ours) (void)change_named_setting(name, *before, false);
    before.reset();
}

bool SlowMotion::set(float speed) {
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
    if (set_step_length(game_speed::step_seconds(rate))) skater_changed_ = true;
    return true;
}

void SlowMotion::release() {
    time_scale_.release();
    max_sim_fps_.release();
    sim_rate_.release();
    // The skater's own step back; while no skater can be reached (a teleport under way), the next call
    // tries again, and the game's own rate is kept for it until then.
    if (skater_changed_ && base_rate_ && set_step_length(game_speed::step_seconds(*base_rate_))) skater_changed_ = false;
    if (!skater_changed_) base_rate_.reset();
}
}

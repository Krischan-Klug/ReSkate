#pragma once
#include "setting_hold.h"
#include <cstdint>
#include <optional>

// The game run at a speed below its own for a while, smoothly (Engine/Game/Settings/game_speed.h):
// its clock slowed by its time scale and its simulation stepped as many times each real second as
// before, each step the shorter. The three native settings are held together (setting_hold.h) and
// given back together.
//
// The local skater goes along: skate. gives a skater its physics step length when its core is built
// (at a spawn and at every teleport) from the physics world's, which follows the simulation rate, and
// never touches it again (measured 2026-10-09 with a write watch; Ghidra build_pose_object,
// world_get_physics_step_length). A rate changed live would leave the skater stepping out of line,
// and one teleported meanwhile would keep the changed step. So while the rate is held the skater's
// step is the held rate's, and once the rate is given back it gets the game's own back.
//
// Never during a multiplayer session: the local simulation must keep the others' pace, so a session
// that starts while it runs ends it. Other skaters (AI skaters) are not followed. Game update thread
// only.
namespace dingosdk {
class GameSpeed {
public:
    // Runs the game at `speed` (below 1; 1 or more gives the game its own speed back). False when
    // it cannot now (a session, the settings not ready): then it runs at its own.
    bool set(float speed);
    // Gives the game its own speed back, and the local skater its own step once it can be reached.
    void release();
    bool held() const noexcept { return time_scale_.held() || sim_rate_.held() || max_sim_fps_.held(); }

private:
    SettingHold time_scale_{"SimulationTime.TimeScale"};
    SettingHold sim_rate_{"SimulationTime.ForceSimRate"};
    SettingHold max_sim_fps_{"SimulationTime.MaxSimFps"};
    std::optional<std::uint32_t> base_rate_; // the game's own simulation rate, while it matters
    bool skater_changed_{};                  // the local skater's step is ours, to be given back
};
}

#pragma once
#include "setting_hold.h"
#include <cstdint>
#include <optional>

// The game run at a speed below its own for a while, smoothly (Engine/Game/Settings/game_speed.h):
// its clock slowed by its time scale and its simulation stepped as many times each real second as
// before, each step the shorter. The three native settings are held together (setting_hold.h)
// and given back together. Never during a multiplayer session: the local simulation must keep the
// others' pace, so a session that starts while it runs ends it. Game update thread only.
namespace dingosdk {
class GameSpeed {
public:
    // Runs the game at `speed` (below 1; 1 or more gives the game its own speed back). False when
    // it cannot now (a session, the settings not ready): then it runs at its own.
    bool set(float speed);
    void release();
    // Whether it holds any of the three now.
    bool held() const noexcept { return time_scale_.held() || sim_rate_.held() || max_sim_fps_.held(); }

private:
    SettingHold time_scale_{"SimulationTime.TimeScale"};
    SettingHold sim_rate_{"SimulationTime.ForceSimRate"};
    SettingHold max_sim_fps_{"SimulationTime.MaxSimFps"};
    std::optional<std::uint32_t> base_rate_; // the game's own simulation rate, while held
};
}

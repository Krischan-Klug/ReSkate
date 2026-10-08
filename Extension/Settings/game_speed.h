#pragma once
#include "setting_hold.h"

// The game run at a speed below its own for a while: its clock slowed by its time scale
// (SimulationTime.TimeScale, held by setting_hold.h and given back). Only the time scale: skate.
// takes a skater's physics step from its simulation rate (SimulationTime.ForceSimRate) when the
// skater spawns or is teleported, never after, so a rate changed live leaves the skater stepping
// out of line with the simulation, and one teleported meanwhile keeps the changed step (measured
// 2026-10-08: teleported at a rate of 68, its step was 14.7 ms until the next teleport). Never
// during a multiplayer session: the local simulation must keep the others' pace, so a session
// that starts while it runs ends it. Game update thread only.
namespace dingosdk {
class GameSpeed {
public:
    // Runs the game at `speed` (below 1; 1 or more gives the game its own speed back). False when
    // it cannot now (a session, the setting not ready): then it runs at its own.
    bool set(float speed);
    void release();
    bool held() const noexcept { return time_scale_.held(); }

private:
    SettingHold time_scale_{"SimulationTime.TimeScale"};
};
}

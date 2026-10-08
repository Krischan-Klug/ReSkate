#pragma once
#include "local_skater.h"
#include "Engine/Game/Skater/skater_body.h"
#include <cstdint>

// The local skater's body (Engine/Game/Skater/skater_body.h), read from the game
// (Engine/Game/Build/20260929/skater_body.h) for any feature that needs it: every physics
// step as it happens, and each body's contacts in it. Read-only; the steps
// come from No Bail's hook on the skeleton response, which runs once per physics step right
// after the step's body contacts.
namespace dingosdk::skater_body {
// Requires the validated build and No Bail started: verifies the layout once.
bool start(std::uintptr_t image_base) noexcept;
bool available() noexcept;

// One physics step of the local skater (local_skater.h, verified again), as the skeleton
// takes it: a wipeout No Bail filtered never happened.
struct Step {
    LocalSkater skater;
    float seconds{}; // how long the step simulates: the game's time, slower than the real one in slow motion
    bool wipeout{};
};
// Physics thread, in the step: reads made here see exactly this step's values.
using StepObserver = void (*)(const Step&) noexcept;
inline constexpr std::size_t max_step_observers = 4;
// False when the observer is already there or all places are taken.
bool add_step_observer(StepObserver observer) noexcept;
// No Bail's skeleton hook: every physics step of any skater's rig, `seconds` long.
void on_physics_step(std::uintptr_t rig, float seconds, bool wipeout) noexcept;

// Any thread: where the local skater's physics step length is kept now (a float, seconds of game
// time), 0 while there is no local skater. Also for diagnostics: Extension/Debug/write_watch.h.
std::uintptr_t step_length_address() noexcept;
// Game update thread: gives the local skater this physics step length (seconds). The game sets it
// only when it builds the skater's core, from the simulation rate then (Extension/Settings/
// game_speed.h follows a rate it changes with it). False while there is no local skater.
bool set_step_length(float seconds) noexcept;

// Everything the latest physics step's contact processing kept of each body; false when it
// is not readable. Exact in a step observer; read outside the physics step (from the client
// tick, say), a step may be half written.
bool read_contacts(const LocalSkater& skater, Contacts& contacts) noexcept;
}

#pragma once
#include "Engine/Game/Abi/linear_transform.h"
#include <cstdint>

// The skater's live state as the game keeps it (Engine/Game/Build/20260929/skater_state.h):
// the physics state the core chose, and off the board what the offboard state says of the body.
// Plain data; Extension/Skater/local_skater_state.h reads it for the local skater.
namespace dingosdk::skater_state {
enum class Mode : std::uint8_t { on_board, on_foot, ragdoll };

// What the offboard state says, on foot and in a ragdoll.
struct Offboard {
    bool ragdoll{};    // a ragdoll substate moves the body: a bail, and also the tumbles and rolls of a
                       // skater still on their feet
    bool in_the_air{}; // feet off the ground: a jump, a fall, a ragdoll in flight or bouncing
};

struct SkaterState {
    bool on_board_in_the_air{}; // the physics state is one of the board's in-the-air states
    bool offboard{};            // the physics state is the off-board one: on foot or in a ragdoll
    bool offboard_known{};      // the offboard state was readable
    Offboard flags;
    // The physics bodies' velocities, metres per second: the board's root, and the skater's pelvis;
    // and the pelvis's spin, radians per second.
    bool motion_known{};
    game::Vec3 board_velocity{}, body_velocity{}, body_spin{};
};

// Whether mode() is known: off the board it takes the offboard state.
constexpr bool mode_known(const SkaterState& state) noexcept { return !state.offboard || state.offboard_known; }
// On the board, on foot, or in a ragdoll. A ragdoll lasts from a bail's wipeout (or the flight
// before it) until the skater stands up (measured 2026-10-06), and also covers rolls on foot.
constexpr Mode mode(const SkaterState& state) noexcept {
    if (!state.offboard) return Mode::on_board;
    return state.offboard_known && state.flags.ragdoll ? Mode::ragdoll : Mode::on_foot;
}
// How the skater moves: with the board while on it, else as the body (on foot, in a ragdoll).
constexpr game::Vec3 velocity(const SkaterState& state) noexcept {
    return mode(state) == Mode::on_board ? state.board_velocity : state.body_velocity;
}
// In the air: on the board by the physics state; off it by the offboard state.
constexpr bool airborne(const SkaterState& state) noexcept {
    return state.on_board_in_the_air || (state.offboard && state.offboard_known && state.flags.in_the_air);
}
}

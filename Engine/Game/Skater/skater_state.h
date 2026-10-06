#pragma once
#include <cstdint>
#include <string_view>

// The skater's live state as the game keeps it (Engine/Game/Build/20260929/skater_state.h):
// the physics state the core chose, and off the board the offboard state: which substate
// moves the body, and what it does. Plain data; Extension/Skater/local_skater_state.h reads
// it for the local skater.
namespace dingosdk::skater_state {
// What moves the body off the board, as the game's offboard substates name it.
enum class Substate : std::uint8_t {
    unknown,     // none of the substate flags raised
    ground,      // FollowGround: standing or walking
    ground_slide,// FollowGroundSlide
    animation,   // FollowAnimation: an animation carries the body
    trajectory,  // FollowTrajectory: launched along a path, e.g. a jump's take-off
    free_fall,   // Falling
    ragdoll,     // FollowRagdoll, FollowAnimatedRagdoll, FollowSimulatedRagdoll: a bail, and
                 // also the tumbles and rolls of a skater still on their feet
};
enum class Mode : std::uint8_t { on_board, on_foot, ragdoll };

// What the offboard state says, on foot and in a ragdoll.
struct Offboard {
    Substate substate{};
    float height_above_ground{}; // metres, capped at 5
    bool in_the_air{};    // feet off the ground: a jump, a fall, a ragdoll in flight or bouncing
    bool falling{};       // moving down, so also sliding on downhill after a landing
    bool landing{};       // the steps of a landing on foot
    bool mounting{};      // getting on the board
    bool off_the_board{}; // just left it: a dismount, or a bail's first part
    bool foot_a_planted{}, foot_b_planted{}; // alternate while walking
};

struct SkaterState {
    std::uint32_t physics_state{};
    std::string_view physics_state_name; // empty for an id not named yet
    bool on_board_in_the_air{}; // the physics state is one of the board's in-the-air states
    bool offboard{};            // the physics state is the off-board one: on foot or in a ragdoll
    bool offboard_known{};      // the offboard state was readable
    Offboard flags;
};

// Whether mode() is known: off the board it takes the offboard state.
constexpr bool mode_known(const SkaterState& state) noexcept { return !state.offboard || state.offboard_known; }
// On the board, on foot, or in a ragdoll. A ragdoll lasts from a bail's wipeout (or the flight
// before it) until the skater stands up (measured 2026-10-06), and also covers rolls on foot.
constexpr Mode mode(const SkaterState& state) noexcept {
    if (!state.offboard) return Mode::on_board;
    return state.offboard_known && state.flags.substate == Substate::ragdoll ? Mode::ragdoll : Mode::on_foot;
}
// In the air: on the board by the physics state; off it by the offboard state.
constexpr bool airborne(const SkaterState& state) noexcept {
    return state.on_board_in_the_air || (state.offboard && state.offboard_known && state.flags.in_the_air);
}

constexpr std::string_view substate_name(Substate substate) noexcept {
    switch (substate) {
    case Substate::ground: return "ground";
    case Substate::ground_slide: return "ground slide";
    case Substate::animation: return "animation";
    case Substate::trajectory: return "trajectory";
    case Substate::free_fall: return "free fall";
    case Substate::ragdoll: return "ragdoll";
    case Substate::unknown: break;
    }
    return "unknown";
}
}

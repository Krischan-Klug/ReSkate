#pragma once
#include "client_source_spawn.h"
#include <array>
#include <cstdint>
#include <string_view>

namespace dingosdk::game::build::v20260929::skater_state {
// Supported SHA-256 fbce74d5e28ef525dbba2cb4adbebc13405bdbd88f31bc940bca45e4ae88b8f9.
// The skater's live state (Engine/Game/Skater/skater_state.h), read-only.

// The physics state the core's selector chose this step (no_bail.h: choose_physics_state).
inline constexpr std::uintptr_t physics_state_offset = 0x1414; // uint32, context
// Every state class returns its id and its name from its vtable (e.g. 0x14482ecd0 returns
// "PHYSICS_STATE_PHYSICS_AIR", its neighbour 200): all 21 of them.
struct PhysicsStateName {
    std::uint32_t id;
    std::string_view name;
};
inline constexpr std::array<PhysicsStateName, 21> physics_state_names{{
    {100, "PHYSICS_GROUND"}, {101, "SLIDE_GROUND"}, {102, "REVERT_GROUND"}, {103, "GROUND_ANIMATION"},
    {104, "SKITCHING"}, {105, "FOLLOW_PATH"},
    {200, "PHYSICS_AIR"}, {201, "KNOWN_AIR"}, {202, "GRIND_TRICK"}, {203, "WALLIE"},
    {300, "WIPEOUT_GROUND"},
    {504, "OFFBOARD"},
    {600, "HANDPLANT"}, {601, "FOOTPLANT"}, {602, "BONELESS"}, {603, "LIPTRICK"}, {604, "ROLL_IN"}, {605, "SLAPPY"},
    {700, "SLEEPING"}, {701, "NONSPECIFIC"}, {702, "TELEPORTING"},
}};
inline constexpr std::array<std::uint32_t, 2> board_air_states{200, 201}; // PHYSICS_AIR, KNOWN_AIR
inline constexpr std::uint32_t offboard_physics_state = 504; // on foot, and through a whole bail

// The core's trick state (its vtable slot +0xb0 returns core+0x3b8) holds the offboard state
// at +0x78: the skater on foot and in a ragdoll (the script native GetOffboardScoring copies
// it out). Each offboard substate class fills it from its vtable (0x1465e8c28 on, each beside
// its name and id): FollowGround (0) 0x14482d6c0, FollowGroundSlide (7) 0x14482d820,
// FollowAnimation (1) 0x14482d550, FollowTrajectory (2) 0x14482dca0, Falling (6) 0x1448186c0,
// FollowRagdoll (3) 0x14482d970, FollowAnimatedRagdoll (4) 0x14482d3b0, FollowSimulatedRagdoll
// (5) 0x14482db00. Each raises exactly one substate flag below, so they name the substate.
inline constexpr std::uintptr_t trick_state_offset = 0x3b8;   // core
inline constexpr std::uintptr_t offboard_state_offset = 0x78; // trick state
// The offboard state's fields; the flags are one byte each, 0 or 1.
inline constexpr std::uintptr_t height_above_ground_offset = 0x160; // float, metres, capped at 5
inline constexpr std::uintptr_t on_ground_offset = 0x187;  // FollowGround and FollowGroundSlide
inline constexpr std::uintptr_t sliding_offset = 0x18b;    // FollowGroundSlide
inline constexpr std::uintptr_t animation_offset = 0x188;  // FollowAnimation
inline constexpr std::uintptr_t trajectory_offset = 0x189; // FollowTrajectory
inline constexpr std::uintptr_t free_fall_offset = 0x18a;  // Falling
inline constexpr std::uintptr_t ragdoll_offset = 0x18c;    // the three ragdoll substates
// The substate's own flags: in the air (its +0x150), and moving down (0 > its velocity's Y,
// so also sliding on downhill after a landing). Landing, mounting, off the board and the
// feet were named from a play session on 2026-10-05.
inline constexpr std::uintptr_t in_the_air_offset = 0x180;
inline constexpr std::uintptr_t falling_offset = 0x18e;
inline constexpr std::uintptr_t landing_offset = 0x178;
inline constexpr std::uintptr_t mounting_offset = 0x16c;
inline constexpr std::uintptr_t off_the_board_offset = 0x16d;
inline constexpr std::uintptr_t foot_a_planted_offset = 0x16a;
inline constexpr std::uintptr_t foot_b_planted_offset = 0x16b;
// The block they lie in, read at once.
inline constexpr std::uintptr_t offboard_block_offset = 0x160;
inline constexpr std::size_t offboard_block_size = 0x30;

// How fast the skater moves: the linear velocity of its physics bodies, read as the SDK's noclip
// reads and writes them (Extension/Skater/client_noclip.cpp). The board's physics: core+0x430 ->
// +0x18 (vtable board_physics_vtable); the skeleton's: the rig (core+0x438) -> +0x2f10 (vtable
// rig_physics_vtable). Each keeps its bodies at +0x20: their count (uint32) at the start, body n at
// n * 0x130, each pointing back to its owner at +0x10 with its velocity (float[3], m/s) at +0x70.
// The board has 9, body 0 its root; the skeleton 26, body n the ragdoll's body n (skater_body.h).
inline constexpr std::uintptr_t board_holder_offset = 0x430;  // core
inline constexpr std::uintptr_t board_physics_offset = 0x18;  // board holder
inline constexpr std::uintptr_t rig_physics_offset = 0x2f10;  // rig
inline constexpr std::uintptr_t physics_bodies_offset = 0x20; // board or rig physics
inline constexpr std::uintptr_t physics_body_size = 0x130;
inline constexpr std::uintptr_t body_owner_offset = 0x10;
inline constexpr std::uintptr_t body_velocity_offset = 0x70;
inline constexpr std::uint32_t board_body_count = 9, rig_body_count = 26;
inline constexpr std::size_t board_root_body = 0;
inline constexpr std::uintptr_t board_physics_vtable = client_source_spawn::board_physics_vtable;
inline constexpr std::uintptr_t rig_physics_vtable = client_source_spawn::rig_physics_vtable;
}

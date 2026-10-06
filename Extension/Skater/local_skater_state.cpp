#include "local_skater_state.h"
#include "Engine/Core/Platform/memory.h"
#include "Engine/Game/Build/addresses.h"
#include "Engine/Game/Build/20260929/skater_state.h"
#include "Engine/Game/Skater/skater_body.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace dingosdk::skater_state {
namespace {
namespace build = addr::skater_state;

std::string_view physics_state_name(std::uint32_t id) noexcept {
    const auto& names = build::physics_state_names;
    const auto known = std::find_if(names.begin(), names.end(), [id](const auto& state) { return state.id == id; });
    return known == names.end() ? std::string_view{} : known->name;
}

// Body `index` of the board's or the skeleton's physics: its velocity, when the layout holds.
bool body_velocity(std::uintptr_t physics, std::uintptr_t vtable, std::uint32_t count, std::size_t index,
    game::Vec3& velocity) noexcept {
    std::uintptr_t type{}, bodies{}, owner{};
    std::uint32_t found{};
    const auto body = [&] { return bodies + index * build::physics_body_size; };
    if (!memory::peek(physics, type) || type != vtable || !memory::peek(physics + build::physics_bodies_offset, bodies) ||
        !memory::peek(bodies, found) || found != count || index >= count ||
        !memory::peek(body() + build::body_owner_offset, owner) || owner != physics ||
        !memory::peek(body() + build::body_velocity_offset, velocity))
        return false;
    return std::all_of(velocity.begin(), velocity.end(), [](float v) { return std::isfinite(v) && std::abs(v) < 1000; });
}
// The board's root and the pelvis.
bool read_motion(const LocalSkater& skater, SkaterState& state) noexcept {
    std::uintptr_t holder{}, board{}, rig{};
    return memory::peek(skater.core + build::board_holder_offset, holder) &&
        memory::peek(holder + build::board_physics_offset, board) &&
        memory::peek(skater.rig + build::rig_physics_offset, rig) &&
        body_velocity(board, skater.base + build::board_physics_vtable, build::board_body_count, build::board_root_body,
            state.board_velocity) &&
        body_velocity(rig, skater.base + build::rig_physics_vtable, build::rig_body_count, skater_body::index(skater_body::Bone::hips),
            state.body_velocity);
}

bool read_offboard(std::uintptr_t core, Offboard& flags) noexcept {
    std::uintptr_t trick_state{}, offboard{};
    std::array<std::uint8_t, build::offboard_block_size> block{};
    if (!memory::peek(core + build::trick_state_offset, trick_state) ||
        !memory::peek(trick_state + build::offboard_state_offset, offboard) ||
        !memory::peek(offboard + build::offboard_block_offset, block))
        return false;
    const auto flag = [&](std::uintptr_t offset) { return block[offset - build::offboard_block_offset] != 0; };
    std::memcpy(&flags.height_above_ground, block.data() + (build::height_above_ground_offset - build::offboard_block_offset),
        sizeof(float));
    // Each substate raises exactly one of its flags (the ground slide also the ground's).
    flags.substate = flag(build::ragdoll_offset) ? Substate::ragdoll
        : flag(build::free_fall_offset) ? Substate::free_fall
        : flag(build::trajectory_offset) ? Substate::trajectory
        : flag(build::animation_offset) ? Substate::animation
        : flag(build::sliding_offset) ? Substate::ground_slide
        : flag(build::on_ground_offset) ? Substate::ground
        : Substate::unknown;
    flags.in_the_air = flag(build::in_the_air_offset);
    flags.falling = flag(build::falling_offset);
    flags.landing = flag(build::landing_offset);
    flags.mounting = flag(build::mounting_offset);
    flags.off_the_board = flag(build::off_the_board_offset);
    flags.foot_a_planted = flag(build::foot_a_planted_offset);
    flags.foot_b_planted = flag(build::foot_b_planted_offset);
    return true;
}
}

bool read(const LocalSkater& skater, SkaterState& state) noexcept {
    state = {};
    if (!memory::peek(skater.context + build::physics_state_offset, state.physics_state)) return false;
    state.physics_state_name = physics_state_name(state.physics_state);
    state.on_board_in_the_air = std::find(build::board_air_states.begin(), build::board_air_states.end(), state.physics_state) !=
        build::board_air_states.end();
    state.offboard = state.physics_state == build::offboard_physics_state;
    state.offboard_known = read_offboard(skater.core, state.flags);
    state.motion_known = read_motion(skater, state);
    if (!state.motion_known) state.board_velocity = state.body_velocity = {};
    return true;
}
}

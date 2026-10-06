#include "local_skater_state.h"
#include "Engine/Core/Platform/memory.h"
#include "Engine/Game/Build/addresses.h"
#include "Engine/Game/Build/20260929/skater_state.h"
#include <algorithm>
#include <array>
#include <cstring>

namespace dingosdk::skater_state {
namespace {
namespace build = addr::skater_state;

std::string_view physics_state_name(std::uint32_t id) noexcept {
    const auto& names = build::physics_state_names;
    const auto known = std::find_if(names.begin(), names.end(), [id](const auto& state) { return state.id == id; });
    return known == names.end() ? std::string_view{} : known->name;
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
    return true;
}
}

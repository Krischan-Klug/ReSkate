#include "local_skater_state.h"
#include "Engine/Core/Platform/memory.h"
#include "Engine/Game/Build/addresses.h"
#include "Engine/Game/Build/20260929/no_bail.h"
#include "Engine/Game/Build/20260929/skater_state.h"
#include "Engine/Game/Skater/skater_body.h"
#include <algorithm>
#include <cmath>

namespace dingosdk::skater_state {
namespace {
namespace build = addr::skater_state;

bool finite(const game::Vec3& v) noexcept {
    return std::all_of(v.begin(), v.end(), [](float value) { return std::isfinite(value) && std::abs(value) < 1000; });
}
// Body `index` of the board's or the skeleton's physics, when the layout holds: its velocity, and
// its spin when asked for.
bool body_motion(std::uintptr_t physics, std::uintptr_t vtable, std::uint32_t count, std::size_t index,
    game::Vec3& velocity, game::Vec3* spin = nullptr) noexcept {
    std::uintptr_t type{}, bodies{}, owner{};
    std::uint32_t found{};
    const auto body = [&] { return bodies + index * build::physics_body_size; };
    if (!memory::peek(physics, type) || type != vtable || !memory::peek(physics + build::physics_bodies_offset, bodies) ||
        !memory::peek(bodies, found) || found != count || index >= count ||
        !memory::peek(body() + build::body_owner_offset, owner) || owner != physics ||
        !memory::peek(body() + build::body_velocity_offset, velocity) ||
        (spin && !memory::peek(body() + build::body_spin_offset, *spin)))
        return false;
    return finite(velocity) && (!spin || finite(*spin));
}
// The board's root and the pelvis.
bool read_motion(const LocalSkater& skater, SkaterState& state) noexcept {
    std::uintptr_t holder{}, board{}, rig{};
    return memory::peek(skater.core + build::board_holder_offset, holder) &&
        memory::peek(holder + build::board_physics_offset, board) &&
        memory::peek(skater.rig + build::rig_physics_offset, rig) &&
        body_motion(board, skater.base + build::board_physics_vtable, build::board_body_count, build::board_root_body,
            state.board_velocity) &&
        body_motion(rig, skater.base + build::rig_physics_vtable, build::rig_body_count, skater_body::index(skater_body::Bone::hips),
            state.body_velocity, &state.body_spin);
}

bool read_offboard(std::uintptr_t core, Offboard& flags) noexcept {
    std::uintptr_t trick_state{}, offboard{};
    std::uint8_t ragdoll{}, in_the_air{};
    if (!memory::peek(core + build::trick_state_offset, trick_state) ||
        !memory::peek(trick_state + build::offboard_state_offset, offboard) ||
        !memory::peek(offboard + build::ragdoll_offset, ragdoll) || !memory::peek(offboard + build::in_the_air_offset, in_the_air))
        return false;
    flags.ragdoll = ragdoll != 0;
    flags.in_the_air = in_the_air != 0;
    return true;
}
}

bool read(const LocalSkater& skater, SkaterState& state) noexcept {
    state = {};
    std::uint32_t physics_state{};
    if (!memory::peek(skater.context + build::physics_state_offset, physics_state)) return false;
    state.on_board_in_the_air = std::find(build::board_air_states.begin(), build::board_air_states.end(), physics_state) !=
        build::board_air_states.end();
    state.offboard = physics_state == addr::no_bail::offboard_physics_state;
    state.offboard_known = read_offboard(skater.core, state.flags);
    state.motion_known = read_motion(skater, state);
    if (!state.motion_known) state.board_velocity = state.body_velocity = state.body_spin = {};
    return true;
}
}

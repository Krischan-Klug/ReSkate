#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

// The skater's ragdoll body bones, by the physics bone id the contact processing
// reports (Engine/Game/Build/20260929/body_impacts.h). The ids follow the order of the
// game's physics bone name map (0x140fd6600, one config field per name). The game
// confirms it twice: the contact code's feet-on-board test picks exactly 15, 16, 19 and
// 20, and the skeleton publisher maps each body to the animation joint of its name
// (rig+0x1ae0). Bone 0 is the board's root, never a body hit. The ragdoll has no head body of its own: NECK1, the
// upper neck, is the one the head rides on, so it is shown as the head.
namespace dingosdk::body_bones {
enum class Bone : std::uint8_t {
    board_root, neck1, neck, left_hand, left_forearm, left_arm, left_shoulder,
    right_hand, right_forearm, right_arm, right_shoulder, spine3, spine2, spine1, spine,
    left_toe, left_foot, left_leg, left_upleg, right_toe, right_foot, right_leg, right_upleg, hips,
};
inline constexpr std::size_t count = 24;
inline constexpr std::array<const char*, count> names{
    "Board", "Head", "Neck", "Left hand", "Left forearm", "Left upper arm", "Left collarbone",
    "Right hand", "Right forearm", "Right upper arm", "Right collarbone", "Upper chest", "Chest",
    "Lower back", "Spine", "Left toes", "Left foot", "Left shin", "Left thigh",
    "Right toes", "Right foot", "Right shin", "Right thigh", "Pelvis"};
constexpr std::size_t index(Bone bone) noexcept { return static_cast<std::size_t>(bone); }
constexpr const char* name(Bone bone) noexcept { return names[index(bone)]; }
// The feet touch the ground and the board all the time; their contacts do not tell
// whether a body is still tumbling.
constexpr bool foot(Bone bone) noexcept {
    return bone == Bone::left_toe || bone == Bone::left_foot || bone == Bone::right_toe || bone == Bone::right_foot;
}
}

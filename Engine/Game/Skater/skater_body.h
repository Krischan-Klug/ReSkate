#pragma once
#include "Engine/Game/Abi/linear_transform.h"
#include <array>
#include <cstddef>
#include <cstdint>

// The skater's body as the physics simulates it (Engine/Game/Build/20260929/skater_body.h):
// 24 rigid bodies, the ragdoll, and the contacts of each physics step. Plain data;
// Extension/Skater/local_skater_body.h reads it.
//
// The bodies by the physics bone id the contact processing reports. The ids follow the
// order of the game's physics bone name map (Engine/Game/Build/20260929/skater_body.h). The
// game confirms it twice: the contact code's feet-on-board test picks exactly 15, 16, 19 and
// 20, and the skeleton publisher maps each body to the animation joint of its name
// (rig+0x1ae0). Bone 0 is the board's root, never a body hit. The ragdoll has no head body of
// its own: NECK1, the upper neck, is the one the head rides on, so it is shown as the head.
namespace dingosdk::skater_body {
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

using game::Vec3;

// What a body's hardest contact in one physics step hit, by the kind the game gives the other
// side. Kinds 5 and 11 are the game's, still to be named.
struct HitKinds {
    bool board{}, vehicle{}, world{}; // world: the ground, walls, rails and the rest of the static world
    bool kind_5{}, kind_11{};
};
// One body in one physics step: whether it touches something, and its hardest contact. Bodies
// only have contacts while the ragdoll simulates them (a bail): not while riding or walking.
struct BodyContact {
    bool touching{};        // a contact in this step
    float since_contact{};  // seconds since its last contact
    bool sensitive{};       // its contacts are the sensitive body contact (a bail's)
    Vec3 velocity{};        // its own velocity, metres per second
    float impact{};         // the hardest contact's speed along its normal, metres per second
    bool tracked_point{};   // that contact was near one of the game's two tracked points
    Vec3 normal{};          // the hardest contact's normal
    Vec3 slide{};           // normal x relative velocity: its length is the speed along the surface
    Vec3 point{};           // where it touched, world space
    HitKinds hit;
};
// The whole body in one physics step.
struct Contacts {
    std::array<BodyContact, count> bodies{};
    bool any{};           // some body touches something
    bool sensitive{};     // a sensitive body touches something: the contact a bail takes
    bool other{};         // a body that is not sensitive touches something
    bool feet_on_board{}; // a toe or foot touches the board
};
// Several physics steps as one, for a reader slower than the physics (a panel, a log): start
// from {} and hold each step. A flag or a hit set in any of them stays set; each body keeps its
// hardest contact with its motion at that moment, or while none hit anything the latest step's.
// The rest (since the last contact, sensitive) is the latest step's.
constexpr void hold(Contacts& held, const Contacts& step) noexcept {
    held.any = held.any || step.any;
    held.sensitive = held.sensitive || step.sensitive;
    held.other = held.other || step.other;
    held.feet_on_board = held.feet_on_board || step.feet_on_board;
    for (std::size_t body = 0; body < count; ++body) {
        auto& h = held.bodies[body];
        const auto& s = step.bodies[body];
        h.touching = h.touching || s.touching;
        h.since_contact = s.since_contact;
        h.sensitive = s.sensitive;
        h.hit = {h.hit.board || s.hit.board, h.hit.vehicle || s.hit.vehicle, h.hit.world || s.hit.world,
            h.hit.kind_5 || s.hit.kind_5, h.hit.kind_11 || s.hit.kind_11};
        if (s.impact < h.impact) continue;
        h.velocity = s.velocity;
        h.impact = s.impact;
        h.tracked_point = s.tracked_point;
        h.normal = s.normal;
        h.slide = s.slide;
        h.point = s.point;
    }
}
// The body with the hardest impact in the step, 0 (the board's root) when none hit anything.
constexpr std::size_t hardest(const Contacts& contacts) noexcept {
    std::size_t best{};
    for (std::size_t body = 1; body < count; ++body)
        if (contacts.bodies[body].impact > contacts.bodies[best].impact) best = body;
    return best;
}
}

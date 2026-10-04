#pragma once
#include "Engine/Game/Skater/body_bones.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// Hall of Meat, the skeleton's shape: the bones to draw for the skater's ragdoll, from
// each body's joint and the body it hangs from (Engine/Game/Build/20260929/
// body_impacts.h). No game access.
namespace dingosdk::hall_of_meat {
using Vec3 = std::array<float, 3>;
// A rigid transform as the game stores it: three basis rows and the position, four floats
// each (the fourth unused).
using Frame = std::array<std::array<float, 4>, 4>;
using Parents = std::array<std::int8_t, body_bones::count>; // each body's parent body, -1 for none

// Where a point given in a frame lands: the game's row-vector convention.
Vec3 place(const Vec3& point, const Frame& frame) noexcept;
// The parents hang together as a tree of bodies.
bool valid_tree(const Parents& parents) noexcept;

struct Ragdoll {
    std::array<Vec3, body_bones::count> joints{}; // world space
    Parents parents{};
};
struct Segment {
    std::size_t body{}; // whose bone it is
    Vec3 from{}, to{};
};
struct Skull {
    Vec3 centre{};
    float radius{};
};
inline constexpr float skull_radius = 0.1f;   // metres
inline constexpr float skull_offset = 0.11f;  // from the head body's joint, along the neck
inline constexpr float end_extension = 0.5f;  // hands and toes, of the bone before them

// A body's bone runs from its joint to each child body's joint (the board, body 0, is not
// drawn). Hands and toes end their chains: their bone continues along the bone before
// them. The head body ends the neck, with the skull sitting on it.
std::vector<Segment> bones(const Ragdoll& ragdoll, Skull& skull);
}

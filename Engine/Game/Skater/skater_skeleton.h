#pragma once
#include "skater_body.h"
#include "Engine/Resource/mesh_set.h"
#include "Engine/Resource/skeleton_asset.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

// The skater's skeleton mesh (skate.'s own, Engine/Game/Build/20260929/skater_skeleton.h), posed
// by the renderer's own skinning matrices (Extension/HallOfMeat/hall_of_meat_render.h), so it lies
// where the game draws the skater. Each vertex knows the ragdoll's body (skater_body.h) it moves
// with, for whatever that body went through. No game access.
namespace dingosdk::skater_skeleton {
using game::LinearTransform;
using game::Vec3;

// The render bone each body poses, by name; the game's skeleton publisher maps them the same way
// (rig+0x1ae0: 102 Neck1, 101 Neck, 278 LeftHand, ... 7 Hips). The board's root has none.
inline constexpr std::array<std::string_view, skater_body::count> joint_names{
    "", "Neck1", "Neck", "LeftHand", "LeftForeArm", "LeftArm", "LeftShoulder",
    "RightHand", "RightForeArm", "RightArm", "RightShoulder", "Spine3", "Spine2", "Spine1", "Spine",
    "LeftToeBase", "LeftFoot", "LeftLeg", "LeftUpLeg", "RightToeBase", "RightFoot", "RightLeg", "RightUpLeg", "Hips"};
inline constexpr std::size_t max_influences = 4; // bones per vertex, the heaviest

struct Vertex {
    Vec3 position{}, normal{}; // model space
    std::array<std::uint16_t, max_influences> bones{};
    std::array<float, max_influences> weights{}; // summing to 1, the unused ones 0
};
struct Mesh {
    std::size_t bone_count{}; // the render skeleton's: a pose has a skinning matrix for each
    std::vector<Vertex> vertices;
    std::vector<std::uint8_t> parts;      // each vertex's body: its heaviest bone's own or nearest ancestor's joint
    std::vector<std::uint32_t> triangles; // vertex indices, three a triangle
};
// Binds a mesh skinned on `skeleton`. Throws when the skeleton lacks a body's joint, or a vertex
// rides on a bone no body moves (the root, the board) or does not fit it.
[[nodiscard]] Mesh bind(const frostbite::Skeleton& skeleton, const frostbite::SkinnedMesh& mesh);

struct Posed {
    std::vector<Vec3> positions, normals; // world space, one per vertex
};
// The mesh skinned by `skin`, each render bone's skinning matrix (a vertex from model space into
// the world). False, leaving `posed` as it was, when it lacks some.
bool pose(const Mesh& mesh, std::span<const LinearTransform> skin, Posed& posed);
}

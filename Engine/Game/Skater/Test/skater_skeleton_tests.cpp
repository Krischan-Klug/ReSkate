// The skater skeleton: a made-up skeleton and mesh bound and skinned.
#include "Engine/Game/Skater/skater_skeleton.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace dingosdk;
using namespace dingosdk::skater_skeleton;
using skater_body::Bone;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
bool near(float a, float b) { return std::abs(a - b) < 1e-4f; }
LinearTransform at(float x, float y, float z) { return {{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {x, y, z, 1}}}; }

// A root, every body's joint under it, and two bones without a body of their own: the head on the
// upper neck and a finger on the left hand.
frostbite::Skeleton made_skeleton() {
    frostbite::Skeleton skeleton;
    const auto add = [&](std::string name, int parent) {
        skeleton.names.push_back(std::move(name));
        skeleton.parents.push_back(parent);
    };
    add("Reference", -1);
    for (std::size_t body = 1; body < skater_body::count; ++body) add(std::string(joint_names[body]), 0);
    add("Head", 1);           // under Neck1
    add("LeftHandIndex1", 3); // under LeftHand
    return skeleton;
}
std::uint16_t bone(const frostbite::Skeleton& skeleton, std::string_view name) {
    return static_cast<std::uint16_t>(std::find(skeleton.names.begin(), skeleton.names.end(), name) - skeleton.names.begin());
}
frostbite::SkinnedVertex vertex_on(std::uint16_t first, std::uint8_t weight, std::uint16_t second = 0) {
    frostbite::SkinnedVertex vertex;
    vertex.position = {0, 1, 0};
    vertex.bones = {first, second};
    vertex.weights = {weight, static_cast<std::uint8_t>(255 - weight)};
    return vertex;
}

void the_renderers_matrices_pose_it() {
    const auto skeleton = made_skeleton();
    const auto head = bone(skeleton, "Head"), finger = bone(skeleton, "LeftHandIndex1"), hand = bone(skeleton, "LeftHand");
    frostbite::SkinnedMesh source;
    source.vertices = {vertex_on(head, 255), vertex_on(finger, 153, hand), vertex_on(head, 255)};
    source.triangles = {0, 1, 2};
    const auto mesh = bind(skeleton, source);
    const auto neck1 = skater_body::index(Bone::neck1), left_hand = skater_body::index(Bone::left_hand);
    check(mesh.bone_count == skeleton.names.size(), "a skinning matrix for each bone");
    check(mesh.parts == std::vector<std::uint8_t>{static_cast<std::uint8_t>(neck1), static_cast<std::uint8_t>(left_hand),
                            static_cast<std::uint8_t>(neck1)},
        "a vertex shows its bone's body: the head the upper neck's, a finger the hand's");
    check(near(mesh.vertices[1].weights[0], 0.6f) && near(mesh.vertices[1].weights[1], 0.4f), "the weights scaled to 1");

    std::vector<LinearTransform> skin(mesh.bone_count, at(0, 0, 0));
    skin[head] = at(0, 2, 0);
    skin[finger] = at(1, 0, 0);
    Posed posed;
    check(pose(mesh, skin, posed), "posed");
    check(near(posed.positions[0][1], 3.0f), "the head where its matrix puts it");
    check(near(posed.positions[1][0], 0.6f) && near(posed.positions[1][1], 1.0f), "a vertex on two bones goes by their weights");
    skin.pop_back();
    check(!pose(mesh, skin, posed), "too few matrices are refused");
}

void a_vertex_off_the_body_is_refused() {
    const auto skeleton = made_skeleton();
    frostbite::SkinnedMesh source;
    source.vertices = {vertex_on(bone(skeleton, "Reference"), 255)};
    bool refused{};
    try { (void)bind(skeleton, source); } catch (const std::runtime_error&) { refused = true; }
    check(refused, "the root is no body's");
    auto renamed = skeleton;
    renamed.names[bone(renamed, "Hips")] = "Pelvis";
    refused = false;
    try { (void)bind(renamed, {}); } catch (const std::runtime_error&) { refused = true; }
    check(refused, "every body needs its joint");
}
}

int main() {
    try {
        the_renderers_matrices_pose_it();
        a_vertex_off_the_body_is_refused();
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Skater skeleton tests passed.\n";
    return 0;
}

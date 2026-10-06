#include "skater_skeleton.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace dingosdk::skater_skeleton {
namespace {
void require(bool valid, const std::string& message) {
    if (!valid) throw std::runtime_error("Skater skeleton: " + message);
}
Vec3 normalised(const Vec3& v) {
    const float length = game::length(v);
    return length > 1e-8f ? Vec3{v[0] / length, v[1] / length, v[2] / length} : Vec3{0, 1, 0};
}
constexpr int no_body = -1;

// Each bone's body: its own joint's, else its nearest ancestor's; none above the hips.
std::vector<int> bodies_of(const frostbite::Skeleton& skeleton) {
    std::vector<int> body(skeleton.names.size(), no_body);
    for (std::size_t b = 1; b < skater_body::count; ++b) {
        const auto found = std::find(skeleton.names.begin(), skeleton.names.end(), joint_names[b]);
        require(found != skeleton.names.end(), "the skeleton has no " + std::string(joint_names[b]));
        body[static_cast<std::size_t>(found - skeleton.names.begin())] = static_cast<int>(b);
    }
    for (std::size_t bone = 0; bone < body.size(); ++bone) { // parents come first
        const auto parent = skeleton.parents[bone];
        if (body[bone] == no_body && parent >= 0) body[bone] = body[static_cast<std::size_t>(parent)];
    }
    return body;
}
}

Mesh bind(const frostbite::Skeleton& skeleton, const frostbite::SkinnedMesh& mesh) {
    const auto body_of = bodies_of(skeleton);
    Mesh result;
    result.bone_count = skeleton.names.size();
    for (const auto& source : mesh.vertices) {
        // The heaviest few bones, scaled back to 1.
        std::array<std::size_t, 8> order{0, 1, 2, 3, 4, 5, 6, 7};
        std::sort(order.begin(), order.end(), [&](auto a, auto b) { return source.weights[a] > source.weights[b]; });
        Vertex vertex;
        vertex.position = source.position;
        float kept{};
        for (std::size_t slot = 0; slot < max_influences && source.weights[order[slot]]; ++slot) {
            const auto bone = source.bones[order[slot]];
            require(bone < body_of.size(), "a vertex names a bone the skeleton lacks");
            require(body_of[bone] != no_body, "a vertex rides on a bone no body moves");
            vertex.bones[slot] = bone;
            vertex.weights[slot] = source.weights[order[slot]] / 255.0f;
            kept += vertex.weights[slot];
        }
        require(kept > 0, "a vertex has no weight");
        for (auto& weight : vertex.weights) weight /= kept;
        result.parts.push_back(static_cast<std::uint8_t>(body_of[vertex.bones[0]]));
        result.vertices.push_back(vertex);
    }
    require(mesh.triangles.size() % 3 == 0, "a triangle is incomplete");
    for (const auto index : mesh.triangles) require(index < result.vertices.size(), "a triangle names a missing vertex");
    result.triangles = mesh.triangles;
    // Area-weighted smooth normals: the costume's own are packed in a form not worth decoding
    // for an untextured x-ray.
    for (std::size_t i = 0; i < result.triangles.size(); i += 3) {
        auto& a = result.vertices[result.triangles[i]];
        auto& b = result.vertices[result.triangles[i + 1]];
        auto& c = result.vertices[result.triangles[i + 2]];
        const Vec3 u{b.position[0] - a.position[0], b.position[1] - a.position[1], b.position[2] - a.position[2]};
        const Vec3 v{c.position[0] - a.position[0], c.position[1] - a.position[1], c.position[2] - a.position[2]};
        const Vec3 n{u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
        for (auto* vertex : {&a, &b, &c})
            for (std::size_t axis = 0; axis < 3; ++axis) vertex->normal[axis] += n[axis];
    }
    for (auto& vertex : result.vertices) vertex.normal = normalised(vertex.normal);
    return result;
}

bool pose(const Mesh& mesh, std::span<const LinearTransform> skin, Posed& posed) {
    if (skin.size() < mesh.bone_count) return false;
    posed.positions.resize(mesh.vertices.size());
    posed.normals.resize(mesh.vertices.size());
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
        const auto& vertex = mesh.vertices[i];
        Vec3 position{}, normal{};
        for (std::size_t slot = 0; slot < max_influences && vertex.weights[slot] > 0; ++slot) {
            const auto& transform = skin[vertex.bones[slot]];
            const auto p = game::place(vertex.position, transform), n = game::turn(vertex.normal, transform);
            for (std::size_t axis = 0; axis < 3; ++axis) {
                position[axis] += p[axis] * vertex.weights[slot];
                normal[axis] += n[axis] * vertex.weights[slot];
            }
        }
        posed.positions[i] = position;
        posed.normals[i] = normalised(normal);
    }
    return true;
}
}

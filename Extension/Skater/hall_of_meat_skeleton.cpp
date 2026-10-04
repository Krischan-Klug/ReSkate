#include "hall_of_meat_skeleton.h"
#include <cmath>

namespace dingosdk::hall_of_meat {
Vec3 place(const Vec3& point, const Frame& frame) noexcept {
    Vec3 result{};
    for (std::size_t i = 0; i < 3; ++i)
        result[i] = point[0] * frame[0][i] + point[1] * frame[1][i] + point[2] * frame[2][i] + frame[3][i];
    return result;
}

bool valid_tree(const Parents& parents) noexcept {
    for (std::size_t body = 0; body < body_bones::count; ++body) {
        auto ancestor = parents[body];
        for (std::size_t depth = 0; ancestor >= 0; ++depth) {
            if (depth == body_bones::count || static_cast<std::size_t>(ancestor) >= body_bones::count) return false;
            ancestor = parents[static_cast<std::size_t>(ancestor)];
        }
    }
    return true;
}

std::vector<Segment> bones(const Ragdoll& ragdoll, Skull& skull) {
    std::vector<Segment> result;
    skull = {};
    std::array<bool, body_bones::count> has_child{};
    for (std::size_t body = 1; body < body_bones::count; ++body)
        if (ragdoll.parents[body] > 0) has_child[static_cast<std::size_t>(ragdoll.parents[body])] = true;
    const auto head = body_bones::index(body_bones::Bone::neck1);
    for (std::size_t body = 1; body < body_bones::count; ++body) {
        if (ragdoll.parents[body] <= 0) continue;
        const auto parent = static_cast<std::size_t>(ragdoll.parents[body]);
        const auto& from = ragdoll.joints[parent];
        const auto& to = ragdoll.joints[body];
        result.push_back({parent, from, to});
        if (has_child[body]) continue;
        const Vec3 along{to[0] - from[0], to[1] - from[1], to[2] - from[2]};
        if (body == head) {
            const float length = std::sqrt(along[0] * along[0] + along[1] * along[1] + along[2] * along[2]);
            if (length < 1e-4f) continue;
            for (std::size_t i = 0; i < 3; ++i) skull.centre[i] = to[i] + along[i] / length * skull_offset;
            skull.radius = skull_radius;
            continue;
        }
        result.push_back({body, to, {to[0] + along[0] * end_extension, to[1] + along[1] * end_extension,
                                     to[2] + along[2] * end_extension}});
    }
    return result;
}
}

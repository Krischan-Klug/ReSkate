#include "overlay_internal.h"
#include <algorithm>
#include <cmath>

// skate.'s skeleton mesh (overlay.h SkeletonFrame) drawn as an x-ray over the world, for any
// feature that shows the skater's skeleton: projected like the nametags, on the background draw
// list, taking no input.

namespace dingosdk::overlay::detail {
namespace {
using Vec3 = std::array<float, 3>;
float dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
// `colour` darkened to `light` (0 black, 1 itself), at `alpha`.
ImU32 lit(ImU32 colour, float light, float alpha) {
    const auto channel = [&](int shift) {
        return static_cast<ImU32>(static_cast<float>((colour >> shift) & 0xff) * std::clamp(light, 0.0f, 1.0f)) << shift;
    };
    const auto a = static_cast<ImU32>(255.0f * std::clamp(alpha, 0.0f, 1.0f));
    return channel(IM_COL32_R_SHIFT) | channel(IM_COL32_G_SHIFT) | channel(IM_COL32_B_SHIFT) | a << IM_COL32_A_SHIFT;
}
}

// Every triangle drawn back to front, each vertex in its body's paint and lit by how it faces the
// camera: an outline opaque and a middle faint (unless the paint is solid), so the bones read
// through each other and against the world.
void draw_skeleton(const SkeletonFrame& skeleton, const std::array<SkeletonPaint, skater_body::count>& paints, float alpha) {
    const auto& positions = skeleton.positions;
    const auto count = positions.size();
    if (!count || count > 0xffff || !skeleton.triangles || !skeleton.parts || skeleton.parts->size() != count ||
        skeleton.normals.size() != count || alpha <= 0 || !(skeleton.vertical_fov > 1 && skeleton.vertical_fov < 175))
        return;
    const auto display = ImGui::GetIO().DisplaySize;
    const auto& m = skeleton.camera;
    const Vec3 right{m[0], m[1], m[2]}, up{m[4], m[5], m[6]}, back{m[8], m[9], m[10]}, origin{m[12], m[13], m[14]};
    const float focal = display.y / (2.0f * std::tan(skeleton.vertical_fov * 3.14159265f / 360.0f));
    const ImVec2 centre(display.x * 0.5f, display.y * 0.5f);
    // Render thread only: reused frame to frame.
    static std::vector<ImVec2> at;
    static std::vector<float> depth;
    static std::vector<ImU32> colour;
    static std::vector<std::pair<float, std::uint32_t>> order; // a triangle's depth, its first index
    at.resize(count);
    depth.resize(count);
    colour.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto& p = positions[i];
        const Vec3 delta{p[0] - origin[0], p[1] - origin[1], p[2] - origin[2]};
        depth[i] = -dot(delta, back);
        const float z = std::max(depth[i], 0.1f);
        at[i] = ImVec2(centre.x + dot(delta, right) * focal / z, centre.y - dot(delta, up) * focal / z);
        const float distance = std::sqrt(dot(delta, delta));
        const float facing = distance > 1e-4f ? std::abs(dot(skeleton.normals[i], delta)) / distance : 1.0f;
        const auto& paint = paints[std::min<std::size_t>((*skeleton.parts)[i], skater_body::count - 1)];
        const float faint = 0.25f + 0.6f * (1.0f - facing);
        colour[i] = lit(paint.colour, 0.55f + 0.45f * facing, alpha * (paint.solid + (1.0f - paint.solid) * faint));
    }
    const auto& triangles = *skeleton.triangles;
    order.clear();
    for (std::uint32_t t = 0; t + 2 < triangles.size(); t += 3) {
        const auto a = triangles[t], b = triangles[t + 1], c = triangles[t + 2];
        if (a >= count || b >= count || c >= count || depth[a] <= 0.1f || depth[b] <= 0.1f || depth[c] <= 0.1f) continue;
        order.emplace_back(depth[a] + depth[b] + depth[c], t);
    }
    if (order.empty()) return;
    std::sort(order.begin(), order.end(), [](const auto& x, const auto& y) { return x.first > y.first; }); // far first
    auto* draw = ImGui::GetBackgroundDrawList();
    const auto uv = ImGui::GetFontTexUvWhitePixel();
    draw->PrimReserve(static_cast<int>(order.size() * 3), static_cast<int>(count));
    const auto first = draw->_VtxCurrentIdx;
    for (std::size_t i = 0; i < count; ++i) draw->PrimWriteVtx(at[i], uv, colour[i]);
    for (const auto& [unused, t] : order)
        for (std::uint32_t corner = 0; corner < 3; ++corner)
            draw->PrimWriteIdx(static_cast<ImDrawIdx>(first + triangles[t + corner]));
}
} // namespace dingosdk::overlay::detail

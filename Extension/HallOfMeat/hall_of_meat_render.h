#pragma once
#include "Engine/Game/Abi/linear_transform.h"
#include <array>
#include <cstdint>
#include <vector>

// The local skater as the renderer draws it (Engine/Game/Build/20260929/skater_render.h): the
// skinning matrices its mesh was drawn with and the camera of the same picture, so Hall of Meat's
// skeleton lies exactly where the game draws the skater. Two hooks that only observe: the skinned
// draw packets, and the main view's camera.
namespace dingosdk::hall_of_meat {
// Requires the validated build: verifies the layout and starts observing.
bool start_render(std::uintptr_t image_base) noexcept;
// Client tick: which render objects are the local skater's (no_bail.h no_bail_skater), riding and walking.
void follow_render() noexcept;

struct Picture {
    std::vector<game::LinearTransform> skin; // each render bone's skinning matrix: from model space into the world
    std::array<float, 16> camera{};          // the camera's world matrix: right, up, back, position rows
    float vertical_fov{};                    // degrees
};
// Render thread: the latest picture's, while the renderer draws the local skater.
bool latest_picture(Picture& picture) noexcept;
}

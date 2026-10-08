#pragma once
#include "Engine/Game/Abi/linear_transform.h"
#include <array>
#include <cstdint>
#include <vector>

// The local skater as the renderer draws it (Engine/Game/Build/20260929/skater_render.h): the
// skinning matrices its mesh was drawn with and the camera of the same picture, for any feature
// that draws over the skater, so it lies exactly where the game draws it. Two hooks that only
// observe: the skinned draw packets, and the main view's camera.
namespace dingosdk::skater_render {
// Requires the validated build: verifies the layout and starts observing.
bool start(std::uintptr_t image_base) noexcept;
bool available() noexcept;
// Client tick, after the local skater was published (local_skater.h): which render objects are
// the local skater's, riding and walking.
void on_client_tick() noexcept;

struct Picture {
    std::vector<game::LinearTransform> skin; // each render bone's skinning matrix: from model space into the world
    std::array<float, 16> camera{};       // the camera's world matrix: right, up, back, position rows
    float vertical_fov{};                 // degrees
};
// Render thread: the latest picture's, while the renderer draws the local skater.
bool latest(Picture& picture) noexcept;
}

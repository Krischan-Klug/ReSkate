#pragma once
#include "Engine/Game/Skater/skater_skeleton.h"
#include <filesystem>
#include <memory>

// The skater's skeleton mesh (Engine/Game/Skater/skater_skeleton.h), read once from the
// installed game's data (Engine/Game/Build/20260929/skater_skeleton.h) for any feature that
// draws the skeleton. Pose it with the skinning matrices local_skater_render.h reads.
namespace dingosdk::skater_skeleton {
// From the game folder (the one with Skate.exe); throws when it cannot be read.
[[nodiscard]] Mesh read_mesh(const std::filesystem::path& game_root);
// Starts reading it in the background, once.
void prepare() noexcept;
// Any thread: the mesh once read; nullptr until then, and for good when it could not be (logged).
std::shared_ptr<const Mesh> mesh() noexcept;
}

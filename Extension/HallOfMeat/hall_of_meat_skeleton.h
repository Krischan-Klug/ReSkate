#pragma once
#include "Engine/Game/Skater/skater_skeleton.h"
#include <filesystem>
#include <memory>

// The skeleton Hall of Meat draws: skate.'s own skeleton mesh (Engine/Game/Skater/skater_skeleton.h),
// read once from the installed game's data (Engine/Game/Build/20260929/skater_skeleton.h) and posed
// with the skinning matrices the renderer drew the skater with (hall_of_meat_render.h).
namespace dingosdk::hall_of_meat {
// From the game folder (the one with Skate.exe); throws when it cannot be read.
[[nodiscard]] skater_skeleton::Mesh read_skeleton_mesh(const std::filesystem::path& game_root);
// Starts reading it in the background, once.
void prepare_skeleton() noexcept;
// Any thread: the mesh once read; nullptr until then, and for good when it could not be (logged).
std::shared_ptr<const skater_skeleton::Mesh> skeleton_mesh() noexcept;
}

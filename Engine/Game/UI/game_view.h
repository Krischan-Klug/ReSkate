#pragma once
#include "Engine/Core/Platform/memory.h"
#include "Engine/Game/Build/addresses.h"
#include "Engine/Game/Build/20260929/client_source_spawn.h"
#include "Engine/Game/Build/20260929/engine.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <optional>

namespace dingosdk {
// The active camera as the client last updated it: its world matrix (rows: right, up,
// back, position) and vertical field of view in degrees. Written on the client thread,
// read by the overlay to place things over the world. `camera` is the camera object itself:
// the game moves its own camera after the client update, so the overlay reads the matrix
// from it again when it draws (a freecam or noclip camera is already final here).
struct GameView {
    std::array<float, 16> world{};
    float vertical_fov{};
    std::uintptr_t camera{};
};
namespace game_view_detail {
inline std::mutex mutex;
inline GameView latest;
inline std::chrono::steady_clock::time_point at;
} // namespace game_view_detail
inline void publish_game_view(const GameView &view) noexcept {
    std::lock_guard lock(game_view_detail::mutex);
    game_view_detail::latest = view;
    game_view_detail::at = std::chrono::steady_clock::now();
}
// Nothing while the camera has not updated recently (loading, no local camera).
inline std::optional<GameView> latest_game_view(std::chrono::milliseconds max_age = std::chrono::milliseconds(250)) noexcept {
    std::lock_guard lock(game_view_detail::mutex);
    if (game_view_detail::at == std::chrono::steady_clock::time_point{} ||
        std::chrono::steady_clock::now() - game_view_detail::at > max_age)
        return std::nullopt;
    return game_view_detail::latest;
}
// The camera as it is now: the game moves its own camera after the client update a view
// was published in, so anything placed with that view trails the picture while the camera
// turns. Refreshes `view` from the same camera object, still of a camera type, with a sound
// matrix; leaves it as it was otherwise. Any thread.
inline void read_live_game_view(std::uintptr_t base, GameView &view) noexcept {
    if (!base || !view.camera || (view.camera & 7)) return;
    std::uintptr_t vtable{};
    if (!memory::peek(view.camera, vtable) ||
        (vtable != base + addr::engine::camera_vtable && vtable != base + addr::client_source_spawn::free_camera_vtable))
        return;
    std::array<float, 16> world{};
    float fov{};
    if (!memory::peek(view.camera + 0x50, world) || !memory::peek(view.camera + 0xac, fov)) return;
    if (!std::isfinite(fov) || fov <= 1 || fov >= 175) return;
    for (const auto value : world)
        if (!std::isfinite(value) || std::abs(value) > 1e7f) return;
    view.world = world;
    view.vertical_fov = fov;
}
} // namespace dingosdk

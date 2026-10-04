#include "custom_nametags.h"
#include "game_ui_state.h"
#include "Engine/Core/Log/logging.h"
#include "Engine/Game/UI/game_view.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>

namespace dingosdk::multiplayer {
namespace {
using Clock = std::chrono::steady_clock;
using Address = std::uintptr_t;

struct State {
    std::atomic<bool> enabled{};
    std::atomic<Address> base{};
    std::mutex mutex;
    // The latest client frame: its camera (published earlier in the same tick) and who
    // stood where, so the two always match.
    GameView view;
    std::vector<overlay::Nametag> tags;
    Clock::time_point published;
    bool hidden{}; // the game hides its own nametags (menus, hidden UI; game_ui_state.h)
};
State &state() {
    static auto *value = new State;
    return *value;
}
} // namespace

void publish_custom_nametags(std::uintptr_t base, std::vector<NametagPlayer> players,
                             std::optional<std::array<float, 3>> local) noexcept {
    auto &s = state();
    if (!s.enabled.load(std::memory_order_acquire)) return;
    s.base.store(base, std::memory_order_release);
    try {
        // This frame's camera, published earlier in the same client tick.
        const auto view = latest_game_view();
        const auto ui = sample_game_ui_state(base);
        const bool hidden = ui.in_menu || ui.ui_hidden || ui.nametags_hidden || ui.indicators_hidden;
        // Distances from the local skater, or from the camera without one.
        if (!local && view) local = std::array<float, 3>{view->world[12], view->world[13], view->world[14]};
        std::vector<overlay::Nametag> tags;
        tags.reserve(players.size());
        for (auto &player : players) {
            overlay::Nametag tag;
            tag.position = player.head;
            tag.name = std::move(player.name);
            tag.color = player.color;
            tag.tag = std::move(player.tag);
            tag.talking = player.talking;
            if (local) {
                const float dx = player.head[0] - (*local)[0], dy = player.head[1] - (*local)[1], dz = player.head[2] - (*local)[2];
                tag.distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            }
            tags.push_back(std::move(tag));
        }
        std::lock_guard lock(s.mutex);
        s.hidden = hidden;
        if (!view) {
            s.tags.clear();
            return;
        }
        s.view = *view;
        s.tags = std::move(tags);
        s.published = Clock::now();
    } catch (...) {}
}

void set_custom_nametags_enabled(bool enabled) noexcept {
    auto &s = state();
    s.enabled.store(enabled, std::memory_order_release);
    if (!enabled) {
        std::lock_guard lock(s.mutex);
        s.tags.clear();
    }
}

bool custom_nametags_enabled() noexcept { return state().enabled.load(std::memory_order_acquire); }

overlay::Nametags custom_nametags() {
    auto &s = state();
    if (!s.enabled.load(std::memory_order_acquire)) return {};
    overlay::Nametags result;
    GameView view;
    {
        std::lock_guard lock(s.mutex);
        if (s.hidden || s.tags.empty() || Clock::now() - s.published > std::chrono::milliseconds(250)) return {};
        result.tags = s.tags;
        view = s.view;
    }
    read_live_game_view(s.base.load(std::memory_order_acquire), view);
    result.camera = view.world;
    result.vertical_fov = view.vertical_fov;
    return result;
}
} // namespace dingosdk::multiplayer

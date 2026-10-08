#pragma once
#include "hall_of_meat_model.h"
#include "Extension/UI/Overlay/overlay.h"
#include <cstdint>
#include <string_view>

// Hall of Meat, as in skate. 3: when the local skater bails, the bones it hurt show over the
// world, yellow where a hit bruised one and red where one broke it, with the bail's card, its
// time and Meat counting until the body comes to rest; both stay a second after the skater gets
// up, then fade out. The hits and the road rash come from the skater body (local_skater_body.h)
// and the ragdoll from the skater state (local_skater_state.h), each physics step as it
// happens; the skeleton is skate.'s own skeleton mesh (skeleton_mesh.h) posed as the renderer
// draws the skater (local_skater_render.h). hall_of_meat_model.h follows the bail, how badly
// each bone was hurt and what it scores. Each map's best Meat is saved with the profile. While
// the card shows it takes the place of skate.'s bottom left HUD, covered
// (Extension/UI/NativeHud/hud_corner.h).
namespace dingosdk::hall_of_meat {
// Requires the validated build, the skater body started, and the local profile loaded: the switch
// starts from the saved choice, on by default.
bool start() noexcept;
// Client thread, every tick: a bail ends when the local skater is gone (a respawn, a teleport);
// logs each finished bail, saves a new best, covers skate.'s HUD corner while the card shows and,
// in single player only, slows the game down a moment after a break (Extension/Settings/game_speed.h).
void on_client_tick() noexcept;
// Client thread, with the level being played (empty without one): loads that map's best when
// the map changes.
void set_level(std::string_view level) noexcept;
// Whether it started (the validated build, the skater body and the local profile).
bool available() noexcept;
bool enabled() noexcept;
// Applies at once and saves the choice with the profile.
void set_enabled(bool enabled) noexcept;
// The overlay's feed (render thread): the skeleton, counter and card to draw now.
overlay::MeatFrame frame();
// Any thread: everything the tracker knows now, for the debug panel (hall_of_meat_debug.h).
Report report() noexcept;
// Client thread: whether the slow motion holds the game's speed settings now (game_speed.h).
bool slowing() noexcept;
// Any thread: the local skater's physics steps as Hall of Meat sees them, for the debug panel.
struct Steps {
    float seconds{};          // the latest step's length, in the game's time
    std::uint64_t count{};    // steps seen since startup
};
Steps steps() noexcept;
}

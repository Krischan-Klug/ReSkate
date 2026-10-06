#pragma once
#include "hall_of_meat_model.h"
#include "Extension/UI/Overlay/overlay.h"
#include <string_view>

// Hall of Meat, as in skate. 3: when the local skater bails, their skeleton shows over the
// world, each bone yellow where a hit bruised it and red where one broke it, with the
// bail's Meat counting up and its card after. The hits and the road rash come from the skater
// body (local_skater_body.h), each physics step as it happens; the skeleton is skate.'s own
// skeleton mesh (skeleton_mesh.h) posed as the renderer draws the skater (local_skater_render.h).
// hall_of_meat_model.h decides what counts as a bail, how badly each bone was hurt and what
// it scores. Each map's best Meat is saved with the profile.
namespace dingosdk::hall_of_meat {
// Requires the validated build, the skater body started, and the local profile loaded: the switch
// starts from the saved choice, on by default.
bool start() noexcept;
// Client thread, with the level being played (empty without one): loads that map's best
// when the map changes, saves a new best, and logs each finished bail.
void on_client_tick(std::string_view level) noexcept;
bool enabled() noexcept;
// Applies at once and saves the choice with the profile.
void set_enabled(bool enabled) noexcept;
// The overlay's feed (render thread): the skeleton, counter and card to draw now.
overlay::MeatFrame frame();
// Any thread: everything the tracker knows now, for the debug panel (hall_of_meat_debug.h).
Report report() noexcept;
}

#pragma once
#include "Extension/UI/Overlay/overlay.h"
#include <cstdint>
#include <string_view>

// Hall of Meat, as in skate. 3: when the local skater bails, their skeleton shows over the
// world, each bone yellow where a hit bruised it and red where one broke it, with the
// bail's Meat counting up and its card after. The impacts come from the game's own
// per-bone contact speeds (Engine/Game/Build/20260929/body_impacts.h), observed through
// No Bail's skeleton hook, and so is the skeleton: the ragdoll's own bodies, in the same
// physics step. hall_of_meat_model.h decides what counts as a bail, how badly each bone
// was hurt and what it scores; hall_of_meat_skeleton.h which bones to draw. Each map's
// best Meat is saved with the profile.
namespace dingosdk::hall_of_meat {
// Requires the validated build, No Bail started, and the local profile loaded: the switch
// starts from the saved choice, on by default.
bool start(std::uintptr_t image_base) noexcept;
// Client thread, with the level being played (empty without one): loads that map's best
// when the map changes, saves a new best, and logs each finished bail.
void on_client_tick(std::string_view level) noexcept;
bool enabled() noexcept;
// Applies at once and saves the choice with the profile.
void set_enabled(bool enabled) noexcept;
// The overlay's feed (render thread): the skeleton, counter and card to draw now.
overlay::MeatFrame frame();
}

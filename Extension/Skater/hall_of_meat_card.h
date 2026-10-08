#pragma once
#include "hall_of_meat_model.h"
#include "Extension/UI/Overlay/overlay.h"
#include <vector>

// Hall of Meat's score card (overlay.h ScoreCard), as in skate. 3: a row per stat with skate.'s
// own icons, then the Thrasher logo, "Hall of Meat" and the Meat, drawn with skate.'s UI shapes.
// No game access.
namespace dingosdk::hall_of_meat {
// When a stat comes onto the card: once the bail has done something worth showing, so a small
// bail's card stays short and a big one grows as it goes. The time shows from the start; the
// Meat counts every stat whether shown or not.
inline constexpr int hits_shown = 5, broken_shown = 5;
inline constexpr float road_rash_shown_feet = 15.0f, airtime_shown_seconds = 3.0f, fall_shown_feet = 100.0f,
    speed_shown_mph = 20.0f, rotations_shown = 2.0f;
// The game images the card shows, its icons, logo and shapes (Engine/Game/Build/20260929/ui_textures.h),
// to add at startup.
std::vector<overlay::GameImage> card_images();
// The card for what the overlay draws now, against the map's best before the bail. No card while
// riding.
overlay::ScoreCard score_card(const View& view, const Standing& against);
}
